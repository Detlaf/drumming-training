#!/usr/bin/env bash
#
# Developer ID sign + notarize + staple a macOS .app bundle.
#
# Prerequisites (one-time, see README "Distribution"):
#   1. Paid Apple Developer Program membership.
#   2. A "Developer ID Application" certificate installed in your login keychain
#      (Xcode > Settings > Accounts > Manage Certificates, or developer.apple.com).
#   3. A stored notarytool credential profile, created once with:
#         xcrun notarytool store-credentials "$NOTARY_PROFILE" \
#             --apple-id "you@example.com" --team-id "ABCDE12345" \
#             --password "app-specific-password"
#      (or use --key/--key-id/--issuer for an App Store Connect API key).
#
# Usage:
#   tools/sign_and_notarize.sh [path/to/App.app]
#
# Environment overrides:
#   IDENTITY        Signing identity. Default: the first "Developer ID
#                   Application" identity found in the keychain.
#   NOTARY_PROFILE  notarytool keychain profile name. Default: drumming-notary.
#   ENTITLEMENTS    Optional path to an entitlements .plist. Default: none.
set -euo pipefail

APP="${1:-build/Drumming.app}"
NOTARY_PROFILE="${NOTARY_PROFILE:-drumming-notary}"

if [[ ! -d "$APP" ]]; then
    echo "error: app bundle not found: $APP" >&2
    exit 1
fi

# ── Resolve signing identity ──────────────────────────────────────────────────
if [[ -z "${IDENTITY:-}" ]]; then
    IDENTITY="$(security find-identity -v -p codesigning \
        | awk -F'"' '/Developer ID Application/ {print $2; exit}')"
fi
if [[ -z "${IDENTITY:-}" ]]; then
    echo "error: no 'Developer ID Application' identity found." >&2
    echo "       Install the certificate, or set IDENTITY=... explicitly." >&2
    echo "       Available identities:" >&2
    security find-identity -v -p codesigning >&2 || true
    exit 1
fi
echo "Signing identity: $IDENTITY"

ENTITLEMENTS_ARGS=()
if [[ -n "${ENTITLEMENTS:-}" ]]; then
    ENTITLEMENTS_ARGS=(--entitlements "$ENTITLEMENTS")
fi

# ── Sign inside-out: nested code first, then the bundle ───────────────────────
# Apple discourages `codesign --deep`; sign each Mach-O dependency explicitly,
# then the outer .app last so its seal covers the already-signed contents.
echo "==> Signing nested frameworks, dylibs, and plugins"
while IFS= read -r -d '' item; do
    codesign --force --timestamp --options runtime \
        --sign "$IDENTITY" "$item"
done < <(find "$APP/Contents" \
            \( -name '*.dylib' -o -name '*.framework' -o -name '*.so' \) -print0)

echo "==> Signing the app bundle"
codesign --force --timestamp --options runtime "${ENTITLEMENTS_ARGS[@]}" \
    --sign "$IDENTITY" "$APP"

echo "==> Verifying signature"
codesign --verify --deep --strict --verbose=2 "$APP"

# ── Notarize ──────────────────────────────────────────────────────────────────
ZIP="$(mktemp -d)/$(basename "${APP%.app}").zip"
echo "==> Zipping for submission: $ZIP"
/usr/bin/ditto -c -k --keepParent "$APP" "$ZIP"

echo "==> Submitting to notary service (profile: $NOTARY_PROFILE)"
xcrun notarytool submit "$ZIP" --keychain-profile "$NOTARY_PROFILE" --wait

echo "==> Stapling ticket"
xcrun stapler staple "$APP"

echo "==> Final Gatekeeper assessment"
spctl --assess --type execute --verbose=4 "$APP"
xcrun stapler validate "$APP"

echo
echo "Done. $APP is signed, notarized, and stapled."
echo "Distribute it inside a .dmg or .zip; recipients can open it without warnings."
