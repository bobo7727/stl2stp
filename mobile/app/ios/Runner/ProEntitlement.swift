import Foundation

/// Pure rule for "does this verified transaction grant Pro". Kept free of
/// StoreKit types so it can be unit tested.
enum ProEntitlement {
  static let productID = "com.stl2stp.app.pro"

  static func grants(
    productID: String, isNonConsumable: Bool, bundleID: String?, expectedBundleID: String?, revoked: Bool
  ) -> Bool {
    productID == Self.productID && isNonConsumable && !revoked
      && bundleID != nil && bundleID == expectedBundleID
  }
}
