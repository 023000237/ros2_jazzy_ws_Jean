# Configuración del comando `robot`

## Instalación

Para habilitar el comando `robot` de forma global, debes agregarlo al archivo de
configuración de tu shell (`~/.bashrc`).

> **Atajo:** el script [`install.sh`](../install.sh) de la raíz del repositorio
> hace esto por ti (además de inicializar los submódulos). Ejecuta `./install.sh`
> y omite los pasos manuales de abajo.

Añade la siguiente función a tu `~/.bashrc`:

```sh
# Habilita el comando robot al entrar en un proyecto de robótica
robot() {
    if [ -f ".docker/scripts/robot" ]; then
        bash .docker/scripts/robot "$@"
    else
        echo "robot: no estás dentro de un proyecto de robótica"
    fi
}
```

Luego recarga la configuración de tu shell:

```sh
source ~/.bashrc
```

## Uso

Después de esto, puedes invocar `robot` desde cualquier directorio.

* Si lo ejecutas dentro de un proyecto de robótica válido (es decir, un
  directorio que contiene `.docker/scripts/robot`), ejecutará el script
  correspondiente.
* En caso contrario, devolverá un mensaje de advertencia indicando que no estás
  dentro de un proyecto válido.

## Notas

* Este enfoque mantiene el comando disponible globalmente sin contaminar el
  `PATH` del sistema.
* El comportamiento es sensible al contexto y depende del directorio de trabajo
  actual.
