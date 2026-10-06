#!/bin/bash

# Get version from git tags or commit count
VERSION=$(git describe --tags --always --dirty)

# Path to your header
HEADER="engine/include/version.h"

# Write the header
echo "#pragma once" > $HEADER
echo "#define PROJECT_VERSION \"${VERSION}\"" >> $HEADER

echo "Wrote version ${VERSION} to ${HEADER}"
