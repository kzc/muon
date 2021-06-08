#include "posix.h"

#include <limits.h>
#include <string.h>

#include "filesystem.h"
#include "log.h"
#include "output.h"
#include "workspace.h"

struct concat_strings_ctx {
	uint32_t *res;
};

#define BUF_SIZE 2048

static bool
concat_str(struct workspace *wk, uint32_t *dest, uint32_t str)
{
	const char *s = wk_str(wk, str);

	if (strlen(s) >= BUF_SIZE) {
		LOG_W(log_out, "string too long in concat strings: '%s'", s);
		return false;
	}

	static char buf[BUF_SIZE + 2] = { 0 };
	uint32_t i = 0;
	bool quote = false;

	for (; *s; ++s) {
		if (*s == ' ') {
			quote = true;
			buf[i] = '$';
			++i;
		} else if (*s == '"') {
			quote = true;
		}

		buf[i] = *s;
		++i;
	}

	buf[i] = 0;
	++i;

	if (quote) {
		wk_str_app(wk, dest, "'");
	}

	wk_str_app(wk, dest, buf);

	if (quote) {
		wk_str_app(wk, dest, "'");
	}

	wk_str_app(wk, dest, " ");
	return true;
}

static bool
concat_strobj(struct workspace *wk, uint32_t *dest, uint32_t src)
{
	struct obj *obj = get_obj(wk, src);
	uint32_t str;

	switch (obj->type) {
	case obj_string:
		str = obj->dat.str;
		break;
	case obj_file:
		str = obj->dat.file;
		break;
	default:
		LOG_W(log_out, "invalid type in concat strings: '%s'", obj_type_to_s(obj->type));
		return ir_err;
	}

	return concat_str(wk, dest, str);
}

static enum iteration_result
concat_strings_iter(struct workspace *wk, void *_ctx, uint32_t val)
{
	struct concat_strings_ctx *ctx = _ctx;
	if (!concat_strobj(wk, ctx->res, val)) {
		return ir_err;
	}

	return ir_cont;
}

static bool
concat_strings(struct workspace *wk, uint32_t arr, uint32_t *res)
{
	if (!*res) {
		*res = wk_str_push(wk, "");
	}

	struct concat_strings_ctx ctx = {
		.res = res,
	};

	return obj_array_foreach(wk, arr, &ctx, concat_strings_iter);
}

static bool
prefix_len(const char *str, const char *prefix, uint32_t *len)
{
	uint32_t l = strlen(prefix);
	if (strncmp(str, prefix, l) == 0) {
		if (strlen(str) > l && str[l] == '/') {
			++l;
		}
		*len = l;
		return true;
	} else {
		*len = 0;
		return false;
	}
}

static uint32_t
longest_prefix_len(const char *str, const char *prefix[])
{
	uint32_t len, max = 0;
	for (; *prefix; ++prefix) {
		if (prefix_len(str, *prefix, &len)) {
			if (len > max) {
				max = len;
			}
		}
	}

	return max;
}

static void
write_hdr(FILE *out, struct workspace *wk, struct project *main_proj)
{
	fprintf(
		out,
		"# This is the build file for project \"%s\"\n"
		"# It is autogenerated by the muon build system.\n"
		"\n"
		"ninja_required_version = 1.7.1\n"
		"\n"
		"# Rules for compiling.\n"
		"\n"
		"rule c_COMPILER\n"
		" command = cc $ARGS -MD -MQ $out -MF $DEPFILE -o $out -c $in\n"
		" deps = gcc\n"
		" depfile = $DEPFILE_UNQUOTED\n"
		" description = Compiling C object $out\n"
		"\n"
		"# Rules for linking.\n"
		"\n"
		"rule STATIC_LINKER\n"
		" command = rm -f $out && gcc-ar $LINK_ARGS $out $in\n"
		" description = Linking static target $out\n"
		"\n"
		"rule c_LINKER\n"
		" command = cc $ARGS -o $out $in $LINK_ARGS\n"
		" description = Linking target $out\n"
		"\n"
		"# Other rules\n"
		"\n"
		"rule CUSTOM_COMMAND\n"
		" command = $COMMAND\n"
		" description = $DESCRIPTION\n"
		" restat = 1\n"
		"\n"
		"# Phony build target, always out of date\n"
		"\n"
		"build PHONY: phony \n"
		"\n"
		"# Build rules for targets\n",
		wk_str(wk, main_proj->cfg.name)
		);
}

struct write_tgt_iter_ctx {
	struct obj *tgt;
	FILE *out;
	uint32_t args_id;
	uint32_t object_names_id;
	uint32_t tgt_dir;
};

static enum iteration_result
write_tgt_sources_iter(struct workspace *wk, void *_ctx, uint32_t val_id)
{
	struct write_tgt_iter_ctx *ctx = _ctx;
	struct obj *src = get_obj(wk, val_id);
	assert(src->type == obj_file); // TODO

	uint32_t pre = longest_prefix_len(wk_str(wk, src->dat.file), (const char *[]) {
		wk_str(wk, ctx->tgt->dat.tgt.cwd),
		wk_str(wk, ctx->tgt->dat.tgt.build_dir),
		NULL,
	});

	uint32_t out;
	out = wk_str_pushf(wk, "%s/%s.o", wk_str(wk, ctx->tgt_dir), &wk_str(wk, src->dat.file)[pre]);

	wk_str_appf(wk, &ctx->object_names_id, "%s ", wk_str(wk, out));

	fprintf(ctx->out,
		"build %s: c_COMPILER %s\n"
		" DEPFILE = %s.d\n"
		" DEPFILE_UNQUOTED = %s.d\n"
		" ARGS = %s\n\n",
		wk_str(wk, out), wk_str(wk, src->dat.file),
		wk_str(wk, out),
		wk_str(wk, out),
		wk_str(wk, ctx->args_id)
		);

	return ir_cont;
}

static enum iteration_result
process_dep_args_includes_iter(struct workspace *wk, void *_ctx, uint32_t inc_id)
{
	struct write_tgt_iter_ctx *ctx = _ctx;
	struct obj *inc = get_obj(wk, inc_id);

	assert(inc->type == obj_file);
	wk_str_appf(wk, &ctx->args_id, "-I%s ", wk_str(wk, inc->dat.file));
	return ir_cont;
}

static enum iteration_result
process_dep_args_iter(struct workspace *wk, void *_ctx, uint32_t val_id)
{
	struct obj *dep = get_obj(wk, val_id);

	if (dep->dat.dep.include_directories) {
		struct obj *inc = get_obj(wk, dep->dat.dep.include_directories);
		assert(inc->type == obj_array);
		if (!obj_array_foreach_flat(wk, dep->dat.dep.include_directories,
			_ctx, process_dep_args_includes_iter)) {
			return ir_err;
		}
	}

	return ir_cont;
}

struct process_link_with_ctx {
	uint32_t *link_args_id;
	uint32_t *implicit_deps_id;
};

static enum iteration_result
process_link_with_iter(struct workspace *wk, void *_ctx, uint32_t val_id)
{
	struct process_link_with_ctx *ctx = _ctx;

	struct obj *tgt = get_obj(wk, val_id);

	if (tgt->type == obj_build_target) {
		wk_str_appf(wk, ctx->implicit_deps_id, " %s/%s",
			wk_str(wk, tgt->dat.tgt.build_dir),
			wk_str(wk, tgt->dat.tgt.build_name));
		wk_str_appf(wk, ctx->link_args_id, " %s/%s",
			wk_str(wk, tgt->dat.tgt.build_dir),
			wk_str(wk, tgt->dat.tgt.build_name));
	} else if (tgt->type == obj_string) {
		wk_str_appf(wk, ctx->link_args_id, " %s", wk_str(wk, tgt->dat.str));
	} else {
		LOG_W(log_out, "invalid type for link_with: '%s'", obj_type_to_s(tgt->type));
		return ir_err;
	}

	return ir_cont;
}

static enum iteration_result
process_dep_links_iter(struct workspace *wk, void *_ctx, uint32_t val_id)
{
	struct obj *dep = get_obj(wk, val_id);

	if (dep->dat.dep.link_with) {
		if (!obj_array_foreach(wk, dep->dat.dep.link_with, _ctx, process_link_with_iter)) {
			return false;
		}
	}

	return ir_cont;
}

static enum iteration_result
process_include_dirs_iter(struct workspace *wk, void *_ctx, uint32_t val_id)
{
	uint32_t *args_id = _ctx;
	struct obj *inc = get_obj(wk, val_id);
	assert(inc->type == obj_file);

	wk_str_appf(wk, args_id, "-I%s ", wk_str(wk, inc->dat.file));
	return ir_cont;
}

struct write_tgt_ctx {
	FILE *out;
	struct project *proj;
};

static enum iteration_result
write_build_tgt(struct workspace *wk, void *_ctx, uint32_t tgt_id)
{
	FILE *out = ((struct write_tgt_ctx *)_ctx)->out;
	struct project *proj = ((struct write_tgt_ctx *)_ctx)->proj;

	struct obj *tgt = get_obj(wk, tgt_id);
	LOG_I(log_out, "writing rules for target '%s'", wk_str(wk, tgt->dat.tgt.build_name));

	uint32_t pre;
	if (!prefix_len(wk_str(wk, tgt->dat.tgt.build_dir), wk->build_root, &pre)) {
		pre = 0;
	}

	uint32_t tgt_dir;
	if (pre == strlen(wk->build_root)) {
		tgt_dir = wk_str_pushf(wk, "%s.p", wk_str(wk, tgt->dat.tgt.build_name));
	} else {
		tgt_dir = wk_str_pushf(wk, "%s/%s.p", &wk_str(wk, tgt->dat.tgt.build_dir)[pre],
			wk_str(wk, tgt->dat.tgt.build_name));
	}

	struct write_tgt_iter_ctx ctx = {
		.tgt = tgt,
		.out = out,
		.tgt_dir = tgt_dir,
	};

	{ /* arguments */
		ctx.args_id = wk_str_pushf(wk, "-I%s -I%s ", wk_str(wk, tgt_dir), wk_str(wk, proj->cwd));

		if (tgt->dat.tgt.include_directories) {
			struct obj *inc = get_obj(wk, tgt->dat.tgt.include_directories);
			assert(inc->type == obj_array);
			if (!obj_array_foreach_flat(wk, tgt->dat.tgt.include_directories, &ctx.args_id, process_include_dirs_iter)) {
				return ir_err;
			}
		}

		{ /* dep includes */
			if (tgt->dat.tgt.deps) {
				if (!obj_array_foreach(wk, tgt->dat.tgt.deps, &ctx, process_dep_args_iter)) {
					return ir_err;
				}
			}
		}

		if (!concat_strings(wk, proj->cfg.args, &ctx.args_id)) {
			return ir_err;
		}

		if (tgt->dat.tgt.c_args) {
			if (!concat_strings(wk, tgt->dat.tgt.c_args, &ctx.args_id)) {
				return ir_err;
			}
		}
	}

	{ /* obj names */
		ctx.object_names_id = wk_str_push(wk, "");
		if (!obj_array_foreach(wk, tgt->dat.tgt.src, &ctx, write_tgt_sources_iter)) {
			return ir_err;
		}
	}

	{ /* target */
		const char *rule;
		uint32_t link_args_id, implicit_deps_id = wk_str_push(wk, "");

		switch (tgt->dat.tgt.type) {
		case tgt_executable:
			rule = "c_LINKER";
			link_args_id = wk_str_push(wk, "-Wl,--as-needed -Wl,--no-undefined");

			{ /* dep links */
				struct process_link_with_ctx ctx = {
					.link_args_id = &link_args_id,
					.implicit_deps_id = &implicit_deps_id
				};

				if (tgt->dat.tgt.deps) {
					if (!obj_array_foreach(wk, tgt->dat.tgt.deps, &ctx, process_dep_links_iter)) {
						return ir_err;
					}
				}

				if (tgt->dat.tgt.link_with) {
					if (!obj_array_foreach(wk, tgt->dat.tgt.link_with, &ctx, process_link_with_iter)) {
						return ir_err;
					}
				}
			}

			break;
		case tgt_library:
			rule = "STATIC_LINKER";
			link_args_id = wk_str_push(wk, "csrD");
			break;
		}

		fprintf(out, "build %s/%s: %s %s | %s",
			wk_str(wk, tgt->dat.tgt.build_dir),
			wk_str(wk, tgt->dat.tgt.build_name),
			rule,
			wk_str(wk, ctx.object_names_id),
			wk_str(wk, implicit_deps_id)
			);
		fprintf(out, "\n LINK_ARGS = %s\n\n", wk_str(wk, link_args_id));
	}

	return ir_cont;
}

static enum iteration_result
custom_tgt_outputs_iter(struct workspace *wk, void *_ctx, uint32_t val_id)
{
	uint32_t *dest = _ctx;

	struct obj *out = get_obj(wk, val_id);
	assert(out->type == obj_file);

	uint32_t pre, str;
	if (prefix_len(wk_str(wk, out->dat.file), wk->build_root, &pre)) {
		str = wk_str_push(wk, &wk_str(wk, out->dat.file)[pre]);
	} else {
		str = out->dat.file;
	}

	return concat_str(wk, dest, str) == true ? ir_cont : ir_err;
}

static enum iteration_result
write_custom_tgt(struct workspace *wk, void *_ctx, uint32_t tgt_id)
{
	FILE *out = ((struct write_tgt_ctx *)_ctx)->out;

	struct obj *tgt = get_obj(wk, tgt_id);
	LOG_I(log_out, "writing rules for custom target '%s'", wk_str(wk, tgt->dat.custom_target.name));

	uint32_t outputs, inputs = 0, cmdline_pre, cmdline = 0;

	if (!concat_strings(wk, tgt->dat.custom_target.input, &inputs)) {
		return ir_err;
	}

	outputs = wk_str_push(wk, "");
	if (!obj_array_foreach(wk, tgt->dat.custom_target.output, &outputs, custom_tgt_outputs_iter)) {
		return ir_err;
	}

	cmdline_pre = wk_str_pushf(wk, "%s internal exe ", wk->argv0);
	if (tgt->dat.custom_target.flags & custom_target_capture) {
		wk_str_app(wk, &cmdline_pre, "-c ");

		uint32_t elem;
		if (!obj_array_index(wk, tgt->dat.custom_target.output, 0, &elem)) {
			return ir_err;
		}

		if (custom_tgt_outputs_iter(wk, &cmdline_pre, elem) == ir_err) {
			return ir_err;
		}
	}

	wk_str_app(wk, &cmdline_pre, "--");

	if (!concat_strings(wk, tgt->dat.custom_target.args, &cmdline)) {
		return ir_err;
	}


	/* desc = wk_str_push(wk, ""); */
	/* if (!concat_str(wk, &desc, wk_str_pushf(wk, "Generating %s with a custom command", wk_str(wk, tgt->dat.custom_target.name)))) { */
	/* 	return ir_err; */
	/* } */

	/* if (tgt->dat.custom_target.flags & custom_target_capture) { */
	/* 	if (!concat_str(wk, &desc, wk_str_push(wk, " (output captured)"))) { */
	/* 		return ir_err; */
	/* 	} */
	/* } */

	fprintf(out, "build %s: CUSTOM_COMMAND %s | %s\n"
		" COMMAND = %s %s\n"
		" DESCRIPTION = %s%s\n",
		wk_str(wk, outputs),
		wk_str(wk, inputs),
		wk_objstr(wk, tgt->dat.custom_target.cmd),

		wk_str(wk, cmdline_pre),
		wk_str(wk, cmdline),
		wk_str(wk, cmdline),
		tgt->dat.custom_target.flags & custom_target_capture ? "(captured)": ""
		);

	return ir_cont;
}

static enum iteration_result
write_tgt_iter(struct workspace *wk, void *_ctx, uint32_t tgt_id)
{
	switch (get_obj(wk, tgt_id)->type) {
	case obj_build_target:
		return write_build_tgt(wk, _ctx, tgt_id);
	case obj_custom_target:
		return write_custom_tgt(wk, _ctx, tgt_id);
	default:
		LOG_W(log_out, "invalid tgt type '%s'", obj_type_to_s(get_obj(wk, tgt_id)->type));
		return ir_err;
	}
}

static bool
write_project(FILE *out, struct workspace *wk, struct project *proj)
{
	struct write_tgt_ctx ctx = { .out = out, .proj = proj };

	if (!obj_array_foreach(wk, proj->targets, &ctx, write_tgt_iter)) {
		return false;
	}

	return true;
}

static FILE *
setup_outdir(const char *dir)
{
	if (!fs_mkdir_p(dir)) {
		return false;
	}

	char path[PATH_MAX + 1] = { 0 };
	snprintf(path, PATH_MAX, "%s/%s", dir, "build.ninja");

	FILE *out;
	if (!(out = fs_fopen(path, "w"))) {
		return NULL;
	}

	return out;
}

bool
output_build(struct workspace *wk, const char *dir)
{
	FILE *out;
	if (!(out = setup_outdir(dir))) {
		return false;
	}

	write_hdr(out, wk, darr_get(&wk->projects, 0));

	uint32_t i;
	for (i = 0; i < wk->projects.len; ++i) {
		if (!write_project(out, wk, darr_get(&wk->projects, i))) {
			return false;
		}
	}

	if (!fs_fclose(out)) {
		return false;
	}

	return true;
}
