#include "posix.h"

#include <string.h>

#include "args.h"
#include "backend/ninja/rules.h"
#include "backend/output.h"
#include "lang/workspace.h"
#include "log.h"
#include "platform/path.h"

static enum iteration_result
write_compiler_rule_iter(struct workspace *wk, void *_ctx, enum compiler_language l, obj comp_id)
{
	FILE *out = _ctx;
	struct obj_compiler *comp = get_obj_compiler(wk, comp_id);

	enum compiler_type t = comp->type;

	const char *deps = NULL;
	switch (compilers[t].deps) {
	case compiler_deps_none:
		break;
	case compiler_deps_gcc:
		deps = "gcc";
		break;
	case compiler_deps_msvc:
		deps = "msvc";
		break;
	}

	obj args;
	make_obj(wk, &args, obj_array);
	obj_array_push(wk, args, comp->name);
	obj_array_push(wk, args, make_str(wk, "$ARGS"));
	if (compilers[t].deps) {
		push_args(wk, args, compilers[t].args.deps("$out", "$DEPFILE"));
	}
	push_args(wk, args, compilers[t].args.output("$out"));
	push_args(wk, args, compilers[t].args.compile_only());
	obj_array_push(wk, args, make_str(wk, "$in"));
	obj command = join_args_plain(wk, args);

	fprintf(out, "rule %s_COMPILER\n"
		" command = %s\n",
		compiler_language_to_s(l),
		get_cstr(wk, command));
	if (compilers[t].deps) {
		fprintf(out,
			" deps = %s\n"
			" depfile = $DEPFILE_UNQUOTED\n",
			deps);
	}
	fprintf(out,
		" description = compiling %s $out\n\n",
		compiler_language_to_s(l));

	fprintf(out, "rule %s_LINKER\n"
		" command = %s $ARGS -o $out $in $LINK_ARGS\n"
		" description = linking $out\n\n",
		compiler_language_to_s(l),
		get_cstr(wk, comp->name));

	return ir_cont;
}

bool
ninja_write_rules(FILE *out, struct workspace *wk, struct project *main_proj, bool need_phony)
{
	fprintf(
		out,
		"# This is the build file for project \"%s\"\n"
		"# It is autogenerated by the muon build system.\n"
		"ninja_required_version = 1.7.1\n\n",
		get_cstr(wk, main_proj->cfg.name)
		);

	// TODO: setup compiler rules for subprojects
	obj_dict_foreach(wk, main_proj->compilers, out, write_compiler_rule_iter);

	fprintf(out,
		"rule STATIC_LINKER\n"
		" command = rm -f $out && ar $LINK_ARGS $out $in\n"
		" description = linking static $out\n"
		"\n"
		"rule CUSTOM_COMMAND\n"
		" command = $COMMAND\n"
		" description = $DESCRIPTION\n"
		" restat = 1\n"
		"\n"
		);

	char setup_file[PATH_MAX];

	if (!path_join(setup_file, PATH_MAX, output_path.private_dir, output_path.setup)) {
		return false;
	}

	obj regen_args;
	make_obj(wk, &regen_args, obj_array);

	obj_array_push(wk, regen_args, make_str(wk, wk->argv0));
	push_args_null_terminated(wk, regen_args, (char *[]) {
		"auto", "-r", "-c", setup_file, NULL
	});

	obj regen_cmd = join_args_shell(wk, regen_args);

	fprintf(out,
		"rule REGENERATE_BUILD\n"
		" command = %s", get_cstr(wk, regen_cmd));

	fputs("\n description = Regenerating build files.\n"
		" generator = 1\n"
		"\n", out);

	fprintf(out,
		"build build.ninja: REGENERATE_BUILD %s\n"
		" pool = console\n"
		"\n"
		"# targets\n\n",
		get_cstr(wk, join_args_ninja(wk, wk->sources))
		);


	if (need_phony) {
		fprintf(out, "build build_always_stale: phony\n\n");
	}

	return true;
}
