import Foundation
import StoreKit

/// Native StoreKit 2 backend for the voluntary tip. It is the only
/// transaction listener / finisher in the iOS app (the Dart `in_app_purchase`
/// backend is not used on iOS).
///
/// - Tip (`com.stl2stp.app.tip.*`, consumable): verified -> finish -> thank-you.
///   Nothing is granted, so no delivery log is kept; `finish()` is idempotent.
/// - Legacy Pro (`com.stl2stp.app.pro`): never sold to anyone (owner's
///   statement, 2026-11-23) and it unlocks nothing, so there is no restore or
///   refund reconciliation. A leftover verified Pro transaction is only
///   finished so it does not replay forever.
actor StoreCoordinator {
  static let shared = StoreCoordinator()

  static let proID = "com.stl2stp.app.pro"
  /// Only the first-release tip is queried and shown. Replay handles every id
  /// here so an unfinished transaction is never left hanging.
  static let tipIDs: Set<String> = ["com.stl2stp.app.tip.small"]

  private var begun = false
  private var buying = false
  private var handledTips = Set<String>()
  private var announcedUnverified = Set<UInt64>()
  private var updatesTask: Task<Void, Never>?
  private var sink: (@Sendable ([String: Any]) -> Void)?

  func setSink(_ sink: (@Sendable ([String: Any]) -> Void)?) { self.sink = sink }

  /// Starts the single Transaction.updates listener and replays unfinished
  /// transactions. Safe to call many times; called at launch.
  func begin() {
    guard !begun else { return }
    begun = true
    updatesTask = Task { [weak self] in
      for await result in Transaction.updates {
        await self?.handle(result)
      }
    }
    Task { [weak self] in
      for await result in Transaction.unfinished {
        await self?.handle(result)
      }
    }
  }

  func loadProducts() async -> [String: Any] {
    let canPay = AppStore.canMakePayments
    do {
      let products = try await Product.products(for: Self.tipIDs)
      let tips = products.filter { $0.type == .consumable && Self.tipIDs.contains($0.id) }.map {
        ["id": $0.id, "title": $0.displayName, "description": $0.description, "price": $0.displayPrice]
      }
      return ["canPay": canPay, "tips": tips]
    } catch {
      return ["canPay": canPay, "tips": [[String: String]](), "error": String(describing: error)]
    }
  }

  func buyTip(productId: String) async -> [String: Any] {
    guard Self.tipIDs.contains(productId) else { return ["outcome": "unavailable"] }
    guard !buying else { return ["outcome": "busy"] }
    buying = true
    defer { buying = false }
    do {
      guard let product = try await Product.products(for: [productId]).first, product.type == .consumable else {
        return ["outcome": "unavailable"]
      }
      switch try await product.purchase() {
      case .success(let verification):
        switch await handle(verification, announce: false) {
        case .tip: return ["outcome": "success"]
        case .unverified: return ["outcome": "unverified"]
        default: return ["outcome": "failed", "message": "unrecognised transaction"]
        }
      case .userCancelled:
        return ["outcome": "cancelled"]
      case .pending:
        return ["outcome": "pending"]
      @unknown default:
        return ["outcome": "failed", "message": "unknown purchase result"]
      }
    } catch {
      if let e = error as? StoreKitError, case .userCancelled = e { return ["outcome": "cancelled"] }
      return ["outcome": "failed", "message": String(describing: error)]
    }
  }

  enum Handled { case tip, unverified, ignored }

  @discardableResult
  private func handle(_ result: VerificationResult<Transaction>, announce: Bool = true) async -> Handled {
    guard case .verified(let t) = result else {
      // Never grant, never finish. Only tell the user when it was a tip.
      if announce, case .unverified(let unverifiedTx, _) = result, Self.tipIDs.contains(unverifiedTx.productID),
        announcedUnverified.insert(unverifiedTx.id).inserted {
        emit(["type": "tip", "status": "unverified"])
      }
      return .unverified
    }
    guard t.appBundleID == Bundle.main.bundleIdentifier else { return .ignored }
    if t.productID == Self.proID {
      await t.finish()
      return .ignored
    }
    if Self.tipIDs.contains(t.productID) {
      guard t.productType == .consumable else { return .ignored }
      let first = handledTips.insert(String(t.id)).inserted
      await t.finish()
      // A refunded tip is finished silently; it never earned a thank-you.
      if announce && first && t.revocationDate == nil { emit(["type": "tip", "status": "thanks"]) }
      return .tip
    }
    return .ignored
  }

  private func emit(_ event: [String: Any]) { sink?(event) }
}
