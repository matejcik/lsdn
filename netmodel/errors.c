/** \file
 * Error formatting functions.
 */
#include "include/errors.h"
#include "include/lsdn.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include "private/lsdn.h"


#define mk_diag_fmt(x, fmt) fmt,

/** List of errors and their format strings. */
static const char* error_fmt[] = {
	lsdn_foreach_problem(mk_diag_fmt)
};

#define CAST(x) ((x)subj->ptr)
/** Convert subject name to string. */
static void format_subject(FILE* out, const struct lsdn_problem_ref *subj)
{
	bool printed = false;
	switch(subj->type){
	case LSDNS_IF:
		fputs(CAST(struct lsdn_if*)->ifname, out);
		break;
	case LSDNS_NET:
		if (CAST(struct lsdn_net*)->name.str) {
			fputs(CAST(struct lsdn_net*)->name.str, out);
			printed = true;
		}
		break;
	case LSDNS_VIRT:
		if (CAST(struct lsdn_virt*)->name.str) {
			fputs(CAST(struct lsdn_virt*)->name.str, out);
			printed = true;
		}
		break;
	case LSDNS_PHYS:
		if (CAST(struct lsdn_phys*)->name.str) {
			fputs(CAST(struct lsdn_phys*)->name.str, out);
			printed = true;
		}
		break;
	case LSDNS_ATTR:
		fputs(CAST(const char*), out);
		printed = true;
		break;
	default:
		break;
	}
	if (!printed)
		fprintf(out, "0x%p", subj);
}

/** Print problem description.
 * Constructs a string describing the problem and writes it out to `out`. */
void lsdn_problem_format(FILE* out, const struct lsdn_problem *problem)
{
	size_t i = 0;
	const char* fmt = error_fmt[problem->code];
	while(*fmt){
		if (*fmt == '%'){
			fmt++;
			assert(*fmt == 'o');
			assert(i < problem->refs_count);
			format_subject(out, &problem->refs[i++]);
		}else{
			fputc(*fmt, out);
		}
		fmt++;
	}
	fputc('\n', stderr);
}

/** Report problems to caller.
 * Walks the list of variadic arguments and invokes the problem callback for each.
 * Also prepares data for the `lsdn_problem_format` function.
 * @param ctx Current context.
 * @param code Problem code.
 * @param ...  Variadic list of pairs of `lsdn_problem_ref_type`
 *   and a pointer to the specified object.
 *   The last element must be a single `LSDNS_END` value. */
void lsdn_problem_report(struct lsdn_context *ctx, enum lsdn_problem_code code, ...)
{
	va_list args;
	va_start(args, code);
	struct lsdn_problem *problem = &ctx->problem;
	problem->refs_count = 0;
	problem->refs = ctx->problem_refs;
	problem->code = code;
	while(1){
		enum lsdn_problem_ref_type subtype = va_arg(args, enum lsdn_problem_ref_type);
		if(subtype == LSDNS_END)
			break;
		assert(problem->refs_count < LSDN_MAX_PROBLEM_REFS);
		problem->refs[problem->refs_count++] =
			(struct lsdn_problem_ref) {subtype, va_arg(args, void*)};
	}
	va_end(args);

	if(ctx->problem_cb)
		ctx->problem_cb(problem, ctx->problem_cb_user);
	ctx->problem_count++;
}

void lsdn_problem_stderr_handler(const struct lsdn_problem *problem, void *user)
{
	lsdn_problem_format(stderr, problem);
}
