#!/usr/bin/env bash
# ============================================================
#  EasyLang (EL1) Installer
#  Builds the interpreter and installs the `el` global command
# ============================================================
set -e

BOLD="\033[1m"
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
CYAN="\033[36m"
RESET="\033[0m"

INSTALL_DIR="/usr/local/bin"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)/src"

echo -e "${CYAN}${BOLD}"
echo "  ███████╗██╗     ██╗"
echo "  ██╔════╝██║     ╚═╝"
echo "  █████╗  ██║     ██╗"
echo "  ██╔══╝  ██║     ██║"
echo "  ███████╗███████╗██║"
echo "  ╚══════╝╚══════╝╚═╝  EasyLang Installer v1.0"
echo -e "${RESET}"

# ── Check for C++ compiler ──────────────────────────────────
if ! command -v g++ &>/dev/null && ! command -v clang++ &>/dev/null; then
    echo -e "${RED}[ERROR] No C++ compiler found. Please install g++ or clang++.${RESET}"
    exit 1
fi

CXX="${CXX:-$(command -v g++ 2>/dev/null || command -v clang++ 2>/dev/null)}"
echo -e "${GREEN}[✓] Compiler: ${CXX}${RESET}"

# ── Compile ─────────────────────────────────────────────────
echo -e "${YELLOW}[…] Compiling EasyLang interpreter...${RESET}"
"$CXX" -O3 -std=c++17 -o /tmp/el_build "${SRC_DIR}/main.cpp"
echo -e "${GREEN}[✓] Compilation successful${RESET}"

# ── Install ─────────────────────────────────────────────────
echo -e "${YELLOW}[…] Installing to ${INSTALL_DIR}/el ...${RESET}"
if [ -w "$INSTALL_DIR" ]; then
    cp /tmp/el_build "${INSTALL_DIR}/el"
    chmod +x "${INSTALL_DIR}/el"
else
    sudo cp /tmp/el_build "${INSTALL_DIR}/el"
    sudo chmod +x "${INSTALL_DIR}/el"
fi
echo -e "${GREEN}[✓] Installed: ${INSTALL_DIR}/el${RESET}"

# ── File association hint ───────────────────────────────────
echo ""
echo -e "${CYAN}${BOLD}━━━ Auto-run .el files ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo ""
echo -e "  To run .el files directly without typing 'el', add a shebang:"
echo -e "  ${YELLOW}#!/usr/bin/env el${RESET}"
echo ""
echo -e "  Or on Linux, register the file type:"
echo -e "  ${YELLOW}echo ':EasyLang:E::el::/usr/local/bin/el:' | sudo tee /proc/sys/fs/binfmt_misc/register${RESET}"
echo ""

# ── Shell completions (bash) ────────────────────────────────
COMPLETION_SCRIPT='# EasyLang (el) bash completion
_el_complete() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    if [[ "$cur" == -* ]]; then
        COMPREPLY=($(compgen -W "-noconsole -faster -optimize -version" -- "$cur"))
    else
        COMPREPLY=($(compgen -f -X "!*.el" -- "$cur"))
    fi
}
complete -F _el_complete el'

BASH_COMP_DIR="$HOME/.bash_completion.d"
mkdir -p "$BASH_COMP_DIR"
echo "$COMPLETION_SCRIPT" > "$BASH_COMP_DIR/el"
echo -e "${GREEN}[✓] Bash completions installed${RESET}"

# ── Verify ──────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}✅  EasyLang EL1 installed successfully!${RESET}"
echo ""
echo -e "  Try it: ${CYAN}el --version${RESET}"
echo -e "  Help:   ${CYAN}el${RESET}"
echo ""
