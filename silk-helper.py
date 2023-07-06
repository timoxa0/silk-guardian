#!/usr/bin/env python3
import sys
from pathlib import Path
import yaml
import jinja2
import os
import usb.core

config_file = Path("/etc/silk.yaml")

config_template = """
/* List of all USB devices you want whitelisted (i.e. ignored) */
static const struct usb_device_id whitelist_table[] = {
    {% for entry in whitelist -%}
        { USB_DEVICE({{ entry }}) },
    {% endfor %}
};

{% if shutdown %}
/* shutdown in case of an unknown device */
#define USE_SHUTDOWN
{% endif %}

{% if shell_command %}
/* run command in case of an unknown device */
#define USE_SHELL_COMMAND

char *kill_command_argv[] = {
    "/usr/bin/bash",
    "-c",
    "{{ shell_command }}",
    NULL,
};

{% endif %}
"""


def fail(e):
    print(e, file=sys.stderr)
    sys.exit(1)


def warn(m):
    print(f"WARNING: {m}", file=sys.stderr)


def generate_config_file():
    config = {}
    config["whitelist"] = []
    for dev in usb.core.find(find_all=True):
        config["whitelist"].append(f"0x{dev.idVendor:04x}, 0x{dev.idProduct:04x}")
    config["shutdown"] = False
    config["shell_command"] = "echo $(date) >> /tmp/silk.txt"

    with config_file.open("w") as f:
        yaml.dump(config, f)
    config_file.chmod(0o600)


def generate_header_file():
    environment = jinja2.Environment()
    template = environment.from_string(config_template)

    print("Generating config.h ...", file=sys.stderr)

    if "PACKAGE_VERSION" in os.environ:
        print(f"#define silk_version \"{os.environ['PACKAGE_VERSION']}\"")
    else:
        git_dir = Path() / Path(".git")
        if not git_dir.resolve().exists():
            print(f"ERROR: Can not set version. Env PACKAGE_VERSION is not set and we are currently not in a git repository ({git_dir.resolve()})", file=sys.stderr)
            sys.exit(1)
        os.system('printf "# define silk_version \\"r%s.%s\\"\n" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"')

    with config_file.open() as f:
        y = yaml.safe_load(f)

    if type(y["whitelist"]) != list:
        fail(f"'whitelist' must be a list of strings. Given: {type(y['whitelist'])}")

    for w in y["whitelist"]:
        if type(w) != str:
            fail(f"'{w}' must be a string")
        if w.count(",") != 1:
            fail(f"Every line must be in the format '0x0000, 0x0000' (, wrong). Found: '{w}'")
        if w.count("0x") != 2:
            fail(f"Every line must be in the format '0x0000, 0x0000' (0x wrong). Found: '{w}'")

    if type(y["shutdown"]) != bool:
        fail(f"'shutdown' must be a boolean. Given: {type(y['shutdown'])}")

    print(template.render(whitelist=y["whitelist"],
                          shutdown=y["shutdown"],
                          shell_command=y["shell_command"]))


if __name__ == '__main__':

    if not os.getuid() == 0:
        fail("Needs to be run as root")
    if not config_file.exists():
        warn("Config file {config_file} not found. Generating one and whitelisting all current devices")
        generate_config_file()
    generate_header_file()
