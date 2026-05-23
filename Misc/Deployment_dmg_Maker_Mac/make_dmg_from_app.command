
#!/bin/bash

set -e

# make sure we are in the correct dir when we double-click a .command file
dir=${0%/*}
if [ -d "$dir" ]; then
  cd "$dir"
fi

### ===== CONFIG =====
APP_NAME="PupilEXT"
APP_BUNDLE="${APP_NAME}.app"
VERSION="0.1.3"
DMG_NAME="${APP_NAME}.dmg"
VOL_NAME="${APP_NAME} v${VERSION} Installer"
STAGING_DIR="dmg_staging"
BACKGROUND_IMG="PupilEXT_dmg_background.png"   # optional, leave empty if not used

# Optional signing identity (leave empty to skip signing)
SIGN_IDENTITY=""

### ===== CHECKS =====
if [ ! -d "$APP_BUNDLE" ]; then
  echo "Error: $APP_BUNDLE not found in current directory."
  exit 1
fi

### ===== MAKE EXECUTABLE (descriptor fix) =====
echo "Ensuring executable permissions..."
chmod -R a+rX "$APP_BUNDLE"

## Ensure main binary is executable (fix common packaging issue)
#MAIN_EXEC=$(defaults read "$APP_BUNDLE/Contents/Info" CFBundleExecutable)
#chmod +x "$APP_BUNDLE/Contents/MacOS/$MAIN_EXEC"
MAIN_EXEC=$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP_BUNDLE/Contents/Info.plist" 2>/dev/null || true)

if [ -z "$MAIN_EXEC" ]; then
  echo "CFBundleExecutable missing, attempting fallback..."
  MAIN_EXEC=$(ls "$APP_BUNDLE/Contents/MacOS/" | head -n 1)
fi

if [ -z "$MAIN_EXEC" ]; then
  echo "Could not determine executable inside app bundle."
  exit 1
fi

chmod +x "$APP_BUNDLE/Contents/MacOS/$MAIN_EXEC"

# this part taken from code by Andy Maloney
# http://asmaloney.com/2013/07/howto/packaging-a-mac-os-x-application-using-a-dmg/
# Check the background image DPI and convert it if it isn't 72x72
_BACKGROUND_IMAGE_DPI_H=`sips -g dpiHeight ${BACKGROUND_IMG} | grep -Eo '[0-9]+\.[0-9]+'`
_BACKGROUND_IMAGE_DPI_W=`sips -g dpiWidth ${BACKGROUND_IMG} | grep -Eo '[0-9]+\.[0-9]+'`

if [ $(echo " $_BACKGROUND_IMAGE_DPI_H != 72.0 " | bc) -eq 1 -o $(echo " $_BACKGROUND_IMAGE_DPI_W != 72.0 " | bc) -eq 1 ]; then
   echo "WARNING: The background image's DPI is not 72.  This will result in distorted backgrounds on Mac OS X 10.7+."
   echo "         I will convert it to 72 DPI for you."
   
   _BACKGROUND_TMP="${BACKGROUND_IMG%.*}"_dpifix."${BACKGROUND_IMG##*.}"

   sips -s dpiWidth 72 -s dpiHeight 72 ${BACKGROUND_IMG} --out ${_BACKGROUND_TMP}
   
   BACKGROUND_IMG="${_BACKGROUND_TMP}"
fi

### ===== OPTIONAL CODE SIGN =====
if [ ! -z "$SIGN_IDENTITY" ]; then
  echo "Signing app..."
  codesign --deep --force --verify --sign "$SIGN_IDENTITY" "$APP_BUNDLE"
fi

### ===== PREPARE STAGING =====
echo "Creating staging directory..."
rm -rf "$STAGING_DIR"
mkdir "$STAGING_DIR"

cp -R "$APP_BUNDLE" "$STAGING_DIR/"
##ln -s /Applications "$STAGING_DIR/Applications" # NOTE: halts due to error, hangs dmg creation and leaves mount on

### ===== INSTALL create-dmg IF NEEDED =====
if ! command -v create-dmg &> /dev/null; then
  echo "Installing create-dmg..."
  if command -v brew &> /dev/null; then
    brew install create-dmg
  else
    echo "Homebrew not found. Install it or install create-dmg manually."
    exit 1
  fi
fi

### ===== BUILD DMG =====
echo "Building DMG..."

DMG_ARGS=(
  --no-internet-enable
  --volname "$VOL_NAME"
  --window-pos 200 120
  --window-size 527 480     #527 429    #800 400
  --icon-size 90
  --icon "$APP_BUNDLE" 100 200
  --app-drop-link 400 200    #600 200
)

if [ -f "$BACKGROUND_IMG" ]; then
  DMG_ARGS+=(--background "$BACKGROUND_IMG")
fi

create-dmg "${DMG_ARGS[@]}" "$DMG_NAME" "$STAGING_DIR/"

### ===== CLEANUP =====
echo "Cleaning up..."
rm -rf "$STAGING_DIR"

echo "DMG created: $DMG_NAME"

exit
