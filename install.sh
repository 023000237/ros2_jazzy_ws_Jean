#!/usr/bin/env bash
#
# install.sh — prepara el repositorio para trabajar:
#   1. Inicializa y actualiza los submódulos (git submodule update --init --recursive)
#   2. Instala el comando 'robot' en tu ~/.bashrc si aún no está
#
# Uso:  ./install.sh
#
set -e

# Directorio del repo (donde vive este script), sin importar desde dónde se llame
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASHRC="$HOME/.bashrc"

# ---------------------------------------------------------------------
# 1. Submódulos
# ---------------------------------------------------------------------
echo "==> Inicializando y actualizando submódulos..."
git -C "$REPO_DIR" submodule update --init --recursive
echo "    Submódulos listos."
echo

# ---------------------------------------------------------------------
# 2. Comando 'robot' en ~/.bashrc
# ---------------------------------------------------------------------
echo "==> Verificando el comando 'robot' en $BASHRC..."

# Detecta la definición de la función robot() (evita falsos positivos en comentarios)
if [ -f "$BASHRC" ] && grep -qE '^[[:space:]]*robot[[:space:]]*\(\)' "$BASHRC"; then
    echo "    El comando 'robot' ya está instalado. Nada que hacer."
else
    echo "    No se encontró. Instalándolo..."
    cat >> "$BASHRC" <<'EOF'

# Habilita el comando robot al entrar en un proyecto de robótica
robot() {
    if [ -f ".docker/scripts/robot" ]; then
        bash .docker/scripts/robot "$@"
    else
        echo "robot: no estás dentro de un proyecto de robótica"
    fi
}
EOF
    echo "    Instalado en $BASHRC."
    echo "    Recarga tu shell para usarlo:  source ~/.bashrc"
fi

source ~/.bashrc
echo
echo "==> Instalación completada."
