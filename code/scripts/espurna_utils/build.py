import os
import shutil
import tempfile

from .version import app_full_version_for_env


# to avoid distributing the original .elf, just extract the debug symbols
# which then can be used /w addr2line (since it would still be an .elf format)
def app_add_extract_debug_symbols(env):
    def builder_generator(target, source, env, for_signature):
        return env.VerboseAction(
            "$OBJCOPY --only-keep-debug --compress-debug-sections $SOURCE $TARGET",
            "Extracting debug symbols from $SOURCE",
        )

    env.Append(
        BUILDERS={
            "ExtractDebugSymbols": env.Builder(
                generator=builder_generator, suffix=".debug", src_suffix=".elf"
            )
        }
    )


# extra builder code to compress our output
def app_add_gzip_file(env):
    def gzip_target(target, source, env):
        import gzip
        import shutil

        with open(str(source[0]), "rb") as input:
            with gzip.open(str(target[0]), "wb") as output:
                shutil.copyfileobj(input, output)

    def builder_generator(target, source, env, for_signature):
        return env.VerboseAction(gzip_target, "Compressing $SOURCE")

    env.Append(
        BUILDERS={
            "GzipFile": env.Builder(
                generator=builder_generator, suffix=".gz", src_suffix=".bin"
            )
        }
    )

    env.GzipFile("$BUILD_DIR/${PROGNAME}.bin")


# emulate .ino concatenation to speed up compilation times
def merge_cpp(target, source, env, encoding="utf-8"):
    with tempfile.TemporaryFile() as tmp:
        tmp.write(b"// !!! Automatically generated file; DO NOT EDIT !!! \n")
        tmp.write(
            '#include "{}"\n'.format(
                env.File("${PROJECT_DIR}/espurna/espurna.h").get_abspath()
            ).encode(encoding)
        )
        for src in source:
            src_include = '#include "{}"\n'.format(src.get_abspath())
            tmp.write(src_include.encode(encoding))

        tmp.seek(0)

        with open(target[0].get_abspath(), "wb") as fobj:
            shutil.copyfileobj(tmp, fobj)

    dep = os.path.join("${BUILD_DIR}", "espurna_single_source", "src", "main.cpp.d")
    env.SetDefault(ESPURNA_SINGLE_SOURCE_DEP=dep)


def single_source_target(name):
    return os.path.join("${BUILD_DIR}", "espurna_single_source", "src", name)


# generate .E file from the .cpp, so we can inspect build flags
def add_preprocess_builder(env):
    env.SetDefault(PREPROCESSCOM=env["CXXCOM"].replace("-c", "-dM -E"))

    def builder_generator(target, source, env, for_signature):
        return env.VerboseAction(
            "$PREPROCESSCOM",
            "Preprocessing $SOURCE",
        )

    env.AppendUnique(
        BUILDERS={
            "PreProcess": env.Builder(
                generator=builder_generator, suffix=".E", src_suffix=".cpp"
            )
        }
    )


# generate .d file for dependency management (possibly stripping out useless lib_deps)
def add_sourcedep_builder(env):
    env.SetDefault(SOURCEDEPCOM=env["CXXCOM"].replace("-c", "-M"))

    def builder_generator(target, source, env, for_signature):
        return env.VerboseAction(
            "$SOURCEDEPCOM",
            "Generating dependencies file for $SOURCE",
        )

    env.AppendUnique(
        BUILDERS={
            "SourceDep": env.Builder(
                generator=builder_generator, suffix=".d", src_suffix=".cpp"
            )
        }
    )


def app_add_builder_single_source(env):
    # generate things in the $BUILD_DIR, so there's no need for any extra clean-up code
    source = single_source_target("main.cpp")
    env.SetDefault(ESPURNA_SINGLE_SOURCE_TARGET=source)

    # allow .E and .d to be generated from the merged file
    add_preprocess_builder(env)
    add_sourcedep_builder(env)

    # substitute a single node instead of building it somewhere else as a lib or extra source dir
    # (...and since we can't seem to modify src_filter specifically for the project dir, only middleware works :/)
    def ignore_node(node):
        if node.name.endswith("main.cpp"):
            return env.File(source)
        return None

    project = env.Dir("${PROJECT_DIR}/espurna")
    env.AddBuildMiddleware(ignore_node, os.path.join(project.get_abspath(), "*.cpp"))
    env.Command(
        source,
        env.Glob("${PROJECT_DIR}/espurna/*.cpp"),
        env.VerboseAction(merge_cpp, "Merging project sources into $TARGET"),
    )


# common name for all our output files (.bin, .elf, .map, etc.)


def firmware_prefix(env):
    return f"espurna-{app_full_version_for_env(env)}"


def firmware_filename(env):
    return "-".join(
        [firmware_prefix(env), env.get("ESPURNA_BUILD_NAME", env["PIOENV"])]
    )


def firmware_destination(env):
    dest = env.get("ESPURNA_BUILD_DESTINATION")

    # implicit default to a local directory
    if not dest:
        dest = "${PROJECT_DIR}/build"
    # its a SCons var
    elif dest.startswith("$"):
        pass
    # due to runtime (?) quirks, we will end up in scripts/
    # without specifying this as relative to the projdir
    elif not dest.startswith("/"):
        dest = f"${{PROJECT_DIR}}/{dest}"

    return env.Dir(dest)


def app_add_target_build_and_copy(env):
    env.Replace(ESPURNA_BUILD_DESTINATION=firmware_destination(env))
    env.Replace(ESPURNA_BUILD_FILENAME=firmware_filename(env))

    app_add_extract_debug_symbols(env)
    env.ExtractDebugSymbols("$BUILD_DIR/${PROGNAME}")

    env.InstallAs(
        "${ESPURNA_BUILD_DESTINATION}/${ESPURNA_BUILD_FILENAME}.bin",
        "$BUILD_DIR/${PROGNAME}.bin",
    )
    for suffix in ("map", "elf.debug"):
        env.InstallAs(
            f"${{ESPURNA_BUILD_DESTINATION}}/debug/${{ESPURNA_BUILD_FILENAME}}.{suffix}",
            f"$BUILD_DIR/${{PROGNAME}}.{suffix}",
        )

    env.Alias("install", "$ESPURNA_BUILD_DESTINATION")
    env.Alias("build-and-copy", ["$BUILD_DIR/${PROGNAME}.bin", "install"])


# NOTICE that .re <-> .re.ipp dependency is tricky, b/c we want these to exist *before* any source is built
# (or, attempted to be built. `projenv` does not exist yet, and so there are no dependecies generated)


def app_add_target_build_re2c(env):
    from SCons.Script import COMMAND_LINE_TARGETS

    def action(target, source, env):
        return env.VerboseAction(
            f"re2c --no-generation-date --case-ranges --conditions -W -Werror -o {target} {source}",
            f"Generating {target}",
        )

    targets = [target for target in COMMAND_LINE_TARGETS if target.endswith(".re.ipp")]

    if targets:
        for target in targets:
            if env.Execute(action(target, target.replace(".re.ipp", ".re"), env)):
                env.Exit(1)
        env.Exit(0)


# c/p giant hack from https://github.com/platformio/platformio-core/issues/4574
# force node path used for signature to be relative to `BUILD_DIR` instead of `PROJECT_DIR`
# thus, forcing CacheDir to share results between multiple environments
def app_patch_cachedir(env):
    from SCons.Node.FS import hash_collect, File

    build_dir = env["BUILD_DIR"]

    def patched_get_cachedir_bsig(self):
        try:
            return self.cachesig
        except AttributeError:
            pass

        children = self.children()
        sigs = [n.get_cachedir_csig() for n in children]

        sigs.append(self.get_contents_sig())
        sigs.append(self.get_path(build_dir))

        result = self.cachesig = hash_collect(sigs)

        return result

    File.get_cachedir_bsig = patched_get_cachedir_bsig


# using modified platform_elf2bin.py, changed locally w/ upstream changes
# ref. https://github.com/esp8266/Arduino/commits/3.1.2/tools/elf2bin.py
# - backport 'Fix syntax warning' at https://github.com/esp8266/Arduino/pull/9034
# - backport 'Use subprocess.run' at https://github.com/esp8266/Arduino/pull/8799
# - only partially backport https://github.com/esp8266/Arduino/pull/7844
#   as it changes eboot blob as well as crc location and .ld script(s)
def app_patch_elf2bin(env):
    KNOWN_VERSIONS = (
        # framework-arduinoespressif8266 @ 3.20704.0 (2.7.4)
        "3_20704_0",
        # framework-arduinoespressif8266 @ 3.30102.0 (3.1.2)
        "3_30102_0",
    )

    platform = env.PioPlatform()

    framework_version = platform.get_package_version("framework-arduinoespressif8266")
    version = framework_version.replace(".", "_")

    if not version in KNOWN_VERSIONS:
        return

    builder = env["BUILDERS"]["ElfToBin"]

    cmd_list = builder.action.cmd_list.split(" ")
    elf2bin = env.File(cmd_list[1])

    cmd_list[1] = env.subst(f"${{PROJECT_DIR}}/scripts/platform_elf2bin_{version}.py")
    builder.action.cmd_list = " ".join(cmd_list)
