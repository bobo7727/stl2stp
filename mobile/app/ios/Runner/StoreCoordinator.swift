import Foundation
import StoreKit

/// Native StoreKit 2 backend (tip + Pro). It is the only transaction listener /
/// finisher in the iOS app (the Dart `in_app_purchase` backend is not used on iOS).
///
/// - Tip (`com.stl2stp.app.tip.*`, consumable): verified -> finish -> thank-you.
///   Nothing is granted, so no delivery log is kept; `finish()` is idempotent.
/// - Pro (`com.stl2stp.app.pro`, non-consumable): the ONLY source of truth is
///   `Transaction.currentEntitlements` (verified, right product/type/bundle,
///   not revoked). It is recomputed at start, on every Pro update (refunds
///   arrive here), after a purchase and after an explicit restore. There is no
///   ledger: Apple's answer replaces the Dart-side cache.
actor StoreCoordinator {
  static let shared = StoreCoordinator()

  static let proID = "com.stl2stp.app.pro"
  /// Only the first-release tip is queried and shown. Replay handles every id
  /// here so an unfinished transaction is never left hanging.
  static let tipIDs: Set<String> = ["com.stl2stp.app.tip.small"]

  private var begun = false
  private var buying = false
  /// Last computed Pro entitlement.
  private var proActive = false
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

  /// Dart start-up: begin listening, then compute Pro from Apple's entitlements.
  func start() async -> [String: Any] {
    begin()
    await recomputePro()
    return proEvent()
  }

  func loadProducts() async -> [String: Any] {
    let canPay = AppStore.canMakePayments
    do {
      let products = try await Product.products(for: Self.tipIDs.union([Self.proID]))
      func info(_ p: Product) -> [String: String] {
        ["id": p.id, "title": p.displayName, "description": p.description, "price": p.displayPrice]
      }
      let tips = products.filter { $0.type == .consumable && Self.tipIDs.contains($0.id) }.map(info)
      var result: [String: Any] = ["canPay": canPay, "tips": tips]
      if let pro = products.first(where: { $0.id == Self.proID && $0.type == .nonConsumable }) {
        result["pro"] = info(pro)
      }
      return result
    } catch {
      return ["canPay": canPay, "tips": [[String: String]](), "error": String(describing: error)]
    }
  }

  /// Explicit restore (the only place AppStore.sync is called: it may prompt for sign-in).
  func restore() async -> [String: Any] {
    do {
      try await AppStore.sync()
    } catch {
      // Sync failed or the user cancelled sign-in: keep whatever we had.
      return ["outcome": "failed", "message": String(describing: error)]
    }
    await recomputePro()
    return ["outcome": proActive ? "restored" : "nothingToRestore", "pro": proActive]
  }

  /// Foreground refresh: reads on-device entitlements only (no sync prompt).
  func refresh() async -> [String: Any] {
    await recomputePro()
    return proEvent()
  }

  private func proEvent() -> [String: Any] { ["type": "pro", "pro": proActive] }

  /// currentEntitlements is the single source of truth for Pro.
  private func recomputePro() async {
    var active = false
    for await result in Transaction.currentEntitlements {
      guard case .verified(let t) = result,
        ProEntitlement.grants(
          productID: t.productID, isNonConsumable: t.productType == .nonConsumable,
          bundleID: t.appBundleID, expectedBundleID: Bundle.main.bundleIdentifier, revoked: t.revocationDate != nil)
      else { continue }
      active = true
    }
    let changed = active != proActive
    proActive = active
    if changed { emit(proEvent()) }
  }

  func buyTip(productId: String) async -> [String: Any] {
    guard Self.tipIDs.contains(productId) else { return ["outcome": "unavailable"] }
    return await purchase(productId, type: .consumable, expect: .tip)
  }

  func buyPro() async -> [String: Any] {
    await purchase(Self.proID, type: .nonConsumable, expect: .pro)
  }

  /// One purchase at a time across tip and Pro (never two purchase sheets).
  private func purchase(_ productId: String, type: Product.ProductType, expect: Handled) async -> [String: Any] {
    guard !buying else { return ["outcome": "busy"] }
    buying = true
    defer { buying = false }
    do {
      guard let product = try await Product.products(for: [productId]).first, product.type == type else {
        return ["outcome": "unavailable"]
      }
      switch try await product.purchase() {
      case .success(let verification):
        let handled = await handle(verification, announce: false)
        if handled == expect { return ["outcome": "success"] }
        if handled == .unverified { return ["outcome": "unverified"] }
        return ["outcome": "failed", "message": "unrecognised transaction"]
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

  enum Handled { case tip, pro, unverified, ignored }

  @discardableResult
  private func handle(_ result: VerificationResult<Transaction>, announce: Bool = true) async -> Handled {
    guard case .verified(let t) = result else {
      // Never grant, never finish. Only tell the user when it was a tip
      // (an unverified Pro transaction simply never becomes an entitlement).
      if announce, case .unverified(let unverifiedTx, _) = result, Self.tipIDs.contains(unverifiedTx.productID),
        announcedUnverified.insert(unverifiedTx.id).inserted {
        emit(["type": "tip", "status": "unverified"])
      }
      return .unverified
    }
    guard t.appBundleID == Bundle.main.bundleIdentifier else { return .ignored }
    if t.productID == Self.proID {
      guard t.productType == .nonConsumable else { return .ignored }
      // Recompute first (a refund/revocation arrives here too), then finish.
      await recomputePro()
      await t.finish()
      return .pro
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
