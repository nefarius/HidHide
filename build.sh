#!/usr/bin/env bash

bash --version 2>&1 | head -n 1

set -eo pipefail
SCRIPT_DIR=$(cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd)

###########################################################################
# CONFIGURATION
###########################################################################

BUILD_PROJECT_FILE="$SCRIPT_DIR/build/_build.csproj"
TEMP_DIRECTORY="$SCRIPT_DIR/.fallout/temp"

DOTNET_GLOBAL_FILE="$SCRIPT_DIR/global.json"
DOTNET_INSTALL_URL="https://dot.net/v1/dotnet-install.sh"
DOTNET_CHANNEL="10.0"

export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_NOLOGO=1

###########################################################################
# EXECUTION
###########################################################################

function FirstJsonValue {
    perl -nle 'print $1 if m{"'"$1"'": "([^"]+)",?}' <<< "${@:2}"
}

DOTNET_INSTALLED_MAJOR=""
if [ -x "$(command -v dotnet)" ]; then
    DOTNET_INSTALLED_MAJOR=$(dotnet --version 2>/dev/null | cut -d. -f1 || true)
fi
DOTNET_HAS_NET10_RUNTIME=0
if [ -x "$(command -v dotnet)" ] && dotnet --list-runtimes 2>/dev/null | grep -Eq '^Microsoft\.NETCore\.App 10\.'; then
    DOTNET_HAS_NET10_RUNTIME=1
fi
if [[ "$DOTNET_INSTALLED_MAJOR" =~ ^[0-9]+$ ]] && [ "$DOTNET_INSTALLED_MAJOR" -ge 10 ] && [ "$DOTNET_HAS_NET10_RUNTIME" -eq 1 ]; then
    export DOTNET_EXE="$(command -v dotnet)"
else
    DOTNET_INSTALL_FILE="$TEMP_DIRECTORY/dotnet-install.sh"
    mkdir -p "$TEMP_DIRECTORY"
    curl -Lsfo "$DOTNET_INSTALL_FILE" "$DOTNET_INSTALL_URL"
    chmod +x "$DOTNET_INSTALL_FILE"

    if [[ -f "$DOTNET_GLOBAL_FILE" ]]; then
        DOTNET_VERSION=$(FirstJsonValue "version" "$(cat "$DOTNET_GLOBAL_FILE")")
        if [[ "$DOTNET_VERSION" == ""  ]]; then
            unset DOTNET_VERSION
        fi
    fi

    DOTNET_DIRECTORY="$TEMP_DIRECTORY/dotnet-unix"
    if [[ -z ${DOTNET_VERSION+x} ]]; then
        "$DOTNET_INSTALL_FILE" --install-dir "$DOTNET_DIRECTORY" --channel "$DOTNET_CHANNEL" --no-path
    else
        "$DOTNET_INSTALL_FILE" --install-dir "$DOTNET_DIRECTORY" --version "$DOTNET_VERSION" --no-path
    fi
    export DOTNET_EXE="$DOTNET_DIRECTORY/dotnet"
    export PATH="$DOTNET_DIRECTORY:$PATH"
fi

echo "Microsoft (R) .NET SDK version $("$DOTNET_EXE" --version)"

"$DOTNET_EXE" build "$BUILD_PROJECT_FILE" /nodeReuse:false /p:UseSharedCompilation=false -nologo -clp:NoSummary --verbosity quiet
"$DOTNET_EXE" run --project "$BUILD_PROJECT_FILE" --no-build -- "$@"
