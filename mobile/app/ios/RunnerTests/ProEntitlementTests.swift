import XCTest

@testable import Runner

final class ProEntitlementTests: XCTestCase {
  private let app = "com.stl2stp.stl2stpApp"

  func testValidProGrants() {
    XCTAssertTrue(
      ProEntitlement.grants(
        productID: "com.stl2stp.app.pro", isNonConsumable: true, bundleID: app, expectedBundleID: app, revoked: false))
  }

  func testRevokedDoesNotGrant() {
    XCTAssertFalse(
      ProEntitlement.grants(
        productID: "com.stl2stp.app.pro", isNonConsumable: true, bundleID: app, expectedBundleID: app, revoked: true))
  }

  func testTipNeverGrantsPro() {
    XCTAssertFalse(
      ProEntitlement.grants(
        productID: "com.stl2stp.app.tip.small", isNonConsumable: false, bundleID: app, expectedBundleID: app,
        revoked: false))
  }

  func testWrongTypeDoesNotGrant() {
    XCTAssertFalse(
      ProEntitlement.grants(
        productID: "com.stl2stp.app.pro", isNonConsumable: false, bundleID: app, expectedBundleID: app, revoked: false))
  }

  func testOtherBundleOrMissingBundleDoesNotGrant() {
    XCTAssertFalse(
      ProEntitlement.grants(
        productID: "com.stl2stp.app.pro", isNonConsumable: true, bundleID: "com.other", expectedBundleID: app,
        revoked: false))
    XCTAssertFalse(
      ProEntitlement.grants(
        productID: "com.stl2stp.app.pro", isNonConsumable: true, bundleID: nil, expectedBundleID: nil, revoked: false))
  }
}
