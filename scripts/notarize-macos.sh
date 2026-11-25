#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 5 ]]; then
  echo 'Usage: notarize-macos.sh BUILD_ZIP OUTPUT_PKG DEVELOPER_ID_APPLICATION DEVELOPER_ID_INSTALLER NOTARY_KEYCHAIN_PROFILE' >&2
  exit 2
fi

build_zip=$1
output_pkg=$2
application_identity=$3
installer_identity=$4
notary_profile=$5

if [[ $(uname -s) != Darwin || "$output_pkg" != *.pkg || -e "$output_pkg" ]]; then
  echo 'Run on macOS with a new .pkg output path.' >&2
  exit 2
fi

working_dir=$(mktemp -d)
trap 'rm -rf -- "$working_dir"' EXIT

ditto -x -k "$build_zip" "$working_dir/unpacked"
source_dir="$working_dir/unpacked/dist"
au="$source_dir/Emotional Particle.component"
vst="$source_dir/Emotional Particle.vst3"
version=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$au/Contents/Info.plist")

for bundle in "$au" "$vst"; do
  codesign --force --sign "$application_identity" --options runtime --timestamp "$bundle"
  codesign --verify --deep --strict --verbose=2 "$bundle"
done

au_dir="$working_dir/payload/Library/Audio/Plug-Ins/Components"
vst_dir="$working_dir/payload/Library/Audio/Plug-Ins/VST3"
mkdir -p "$au_dir" "$vst_dir"
ditto "$au" "$au_dir/Emotional Particle.component"
ditto "$vst" "$vst_dir/Emotional Particle.vst3"

pkgbuild --root "$working_dir/payload" --install-location / \
  --identifier audio.spore.emotionalparticle.installer --version "$version" \
  "$working_dir/payload.pkg"
productbuild --package "$working_dir/payload.pkg" --sign "$installer_identity" \
  "$working_dir/EmotionalParticle.pkg"
xcrun notarytool submit "$working_dir/EmotionalParticle.pkg" \
  --keychain-profile "$notary_profile" --wait --output-format json > "$working_dir/notary.json"
cat "$working_dir/notary.json"
[[ $(plutil -extract status raw -o - "$working_dir/notary.json") == Accepted ]]
xcrun stapler staple "$working_dir/EmotionalParticle.pkg"
xcrun stapler validate "$working_dir/EmotionalParticle.pkg"
pkgutil --check-signature "$working_dir/EmotionalParticle.pkg"
spctl --assess --type install --verbose=2 "$working_dir/EmotionalParticle.pkg"
cp "$working_dir/EmotionalParticle.pkg" "$output_pkg"
