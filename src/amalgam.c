/*
 * SPDX-FileCopyrightText: Stone Tickle <lattis@mochiro.moe>
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "posix.h"

#ifdef __sun
#define __EXTENSIONS__
#endif

#include "args.c"
#include "backend/backend.c"
#include "backend/common_args.c"
#include "backend/ninja.c"
#include "backend/ninja/alias_target.c"
#include "backend/ninja/build_target.c"
#include "backend/ninja/custom_target.c"
#include "backend/ninja/rules.c"
#include "backend/output.c"
#include "cmd_install.c"
#include "cmd_test.c"
#include "coerce.c"
#include "compilers.c"
#include "data/bucket_array.c"
#include "data/darr.c"
#include "data/hash.c"
#include "embedded.c"
#include "error.c"
#include "external/bestline_null.c"
#include "external/libarchive_null.c"
#include "external/libcurl_null.c"
#include "external/samurai_null.c"
#include "formats/ini.c"
#include "formats/lines.c"
#include "formats/tap.c"
#include "functions/array.c"
#include "functions/boolean.c"
#include "functions/both_libs.c"
#include "functions/build_target.c"
#include "functions/common.c"
#include "functions/compiler.c"
#include "functions/configuration_data.c"
#include "functions/custom_target.c"
#include "functions/dependency.c"
#include "functions/dict.c"
#include "functions/disabler.c"
#include "functions/environment.c"
#include "functions/external_program.c"
#include "functions/feature_opt.c"
#include "functions/file.c"
#include "functions/generator.c"
#include "functions/kernel.c"
#include "functions/kernel/build_target.c"
#include "functions/kernel/configure_file.c"
#include "functions/kernel/custom_target.c"
#include "functions/kernel/dependency.c"
#include "functions/kernel/install.c"
#include "functions/kernel/options.c"
#include "functions/kernel/subproject.c"
#include "functions/machine.c"
#include "functions/meson.c"
#include "functions/modules.c"
#include "functions/modules/fs.c"
#include "functions/modules/keyval.c"
#include "functions/modules/pkgconfig.c"
#include "functions/modules/python.c"
#include "functions/modules/sourceset.c"
#include "functions/number.c"
#include "functions/run_result.c"
#include "functions/source_configuration.c"
#include "functions/source_set.c"
#include "functions/string.c"
#include "functions/subproject.c"
#include "guess.c"
#include "install.c"
#include "lang/analyze.c"
#include "lang/eval.c"
#include "lang/fmt.c"
#include "lang/interpreter.c"
#include "lang/lexer.c"
#include "lang/object.c"
#include "lang/parser.c"
#include "lang/serial.c"
#include "lang/string.c"
#include "lang/workspace.c"
#include "log.c"
#include "machine_file.c"
#include "main.c"
#include "options.c"
#include "opts.c"
#include "platform/filesystem.c"
#include "platform/mem.c"
#include "platform/null/rpath_fixer.c"
#include "platform/path.c"
#include "platform/run_cmd.c"
#include "platform/term.c"
#include "platform/uname.c"
#include "rpmvercmp.c"
#include "sha_256.c"
#include "version.c.in"
#include "wrap.c"

#ifdef BOOTSTRAP_HAVE_LIBPKGCONF
#include "external/libpkgconf.c"
#else
#include "external/libpkgconf_null.c"
#endif
