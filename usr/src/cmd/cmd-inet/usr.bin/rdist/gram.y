%{
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include "defs.h"

struct	cmd *cmds = NULL;
struct	cmd *last_cmd;
struct	namelist *last_n;
struct	subcmd *last_sc;

static void append(char *label, struct namelist *files, char *stamp,
    struct subcmd *subcmds);
int yyerror(const char *s);

%}

%term EQUAL	1
%term LP	2
%term RP	3
%term SM	4
%term ARROW	5
%term COLON	6
%term DCOLON	7
%term NAME	8
%term STRING	9
%term INSTALL	10
%term NOTIFY	11
%term EXCEPT	12
%term PATTERN	13
%term SPECIAL	14
%term OPTION	15

%union {
	int intval;
	char *string;
	struct subcmd *subcmd;
	struct namelist *namel;
}

%type <intval> OPTION options
%type <string> NAME STRING
%type <subcmd> INSTALL NOTIFY EXCEPT PATTERN SPECIAL cmdlist cmd
%type <namel> namelist names opt_namelist

%%

file:		  /* VOID */
		| file command
		;

command:	  NAME EQUAL namelist {
			(void) lookup($1, INSERT, $3);
		}
		| namelist ARROW namelist cmdlist {
			insert(NULL, $1, $3, $4);
		}
		| NAME COLON namelist ARROW namelist cmdlist {
			insert($1, $3, $5, $6);
		}
		| namelist DCOLON NAME cmdlist {
			append(NULL, $1, $3, $4);
		}
		| NAME COLON namelist DCOLON NAME cmdlist {
			append($1, $3, $5, $6);
		}
		| error
		;

namelist:	  NAME {
			$$ = makenl($1);
		}
		| LP names RP {
			$$ = $2;
		}
		;

names:		  /* VOID */ {
			$$ = last_n = NULL;
		}
		| names NAME {
			if (last_n == NULL)
				$$ = last_n = makenl($2);
			else {
				last_n->n_next = makenl($2);
				last_n = last_n->n_next;
				$$ = $1;
			}
		}
		;

cmdlist:	  /* VOID */ {
			$$ = last_sc = NULL;
		}
		| cmdlist cmd {
			if (last_sc == NULL)
				$$ = last_sc = $2;
			else {
				last_sc->sc_next = $2;
				last_sc = $2;
				$$ = $1;
			}
		}
		;

cmd:		  INSTALL options opt_namelist SM {
			register struct namelist *nl;

			$1->sc_options = $2 | options;
			if ($3 != NULL) {
				nl = expand($3, E_VARS);
				if (nl && nl->n_next != NULL)
					(void) yyerror(
					    "only one name allowed\n");
				$1->sc_name = nl ? nl->n_name: NULL;
				if (nl)
					free(nl);
			}
			$$ = $1;
		}
		| NOTIFY namelist SM {
			if ($2 != NULL)
				$1->sc_args = expand($2, E_VARS);
			$$ = $1;
		}
		| EXCEPT namelist SM {
			if ($2 != NULL)
				$1->sc_args = expand($2, E_ALL);
			$$ = $1;
		}
		| PATTERN namelist SM {
			struct namelist *nl;
			char *cp, *re_comp();

			/*
			 *	We dup the namelist in $2 because expand()
			 *	destroys the list referred to in its first
			 *	argument.
			 */
			for (nl = expand(dupnl($2), E_VARS); nl != NULL;
				nl = nl->n_next)
				if ((cp = re_comp(nl->n_name)) != NULL)
					(void) yyerror(cp);
			$1->sc_args = expand($2, E_VARS);
			$$ = $1;
		}
		| SPECIAL opt_namelist STRING SM {
			if ($2 != NULL)
				$1->sc_args = expand($2, E_ALL);
			$1->sc_name = $3;
			$$ = $1;
		}
		;

options:	  /* VOID */ {
			$$ = 0;
		}
		| options OPTION {
			$$ |= $2;
		}
		;

opt_namelist:	  /* VOID */ {
			$$ = NULL;
		}
		| namelist {
			$$ = $1;
		}
		;

%%

int	yylineno = 1;
extern	FILE *fin;

int
yylex()
{
	static char yytext[INMAX];
	register int c;
	register char *cp1, *cp2;
	static char quotechars[] = "[]{}*?$";

again:
	switch (c = getc(fin)) {
	case EOF:  /* end of file */
		return(0);

	case '#':  /* start of comment */
		while ((c = getc(fin)) != EOF && c != '\n')
			;
		if (c == EOF)
			return(0);
		/* FALLTHROUGH */
	case '\n':
		yylineno++;
	case ' ':
	case '\t':  /* skip blanks */
		goto again;

	case '=':  /* EQUAL */
		return(EQUAL);

	case '(':  /* LP */
		return(LP);

	case ')':  /* RP */
		return(RP);

	case ';':  /* SM */
		return(SM);

	case '-':  /* -> */
		if ((c = getc(fin)) == '>')
			return(ARROW);
		ungetc(c, fin);
		c = '-';
		break;

	case '"':  /* STRING */
		cp1 = yytext;
		cp2 = &yytext[INMAX - 1];
		for (;;) {
			if (cp1 >= cp2) {
				(void) yyerror("command string too long\n");
				break;
			}
			c = getc(fin);
			if (c == EOF || c == '"')
				break;
			if (c == '\\') {
				if ((c = getc(fin)) == EOF) {
					*cp1++ = '\\';
					break;
				}
			}
			if (c == '\n') {
				yylineno++;
				c = ' '; /* can't send '\n' */
			}
			*cp1++ = c;
		}
		if (c != '"')
			(void) yyerror("missing closing '\"'\n");
		*cp1 = '\0';
		yylval.string = makestr(yytext);
		return(STRING);

	case ':':  /* : or :: */
		if ((c = getc(fin)) == ':')
			return(DCOLON);
		ungetc(c, fin);
		return(COLON);
	}
	cp1 = yytext;
	cp2 = &yytext[INMAX - 1];
	for (;;) {
		if (cp1 >= cp2) {
			(void) yyerror("input line too long\n");
			break;
		}
		if (c == '\\') {
			if ((c = getc(fin)) != EOF) {
				if (any(c, quotechars))
					c |= QUOTE;
			} else {
				*cp1++ = '\\';
				break;
			}
		}
		*cp1++ = c;
		c = getc(fin);
		if (c == EOF || any(c, " \"'\t()=;:\n")) {
			ungetc(c, fin);
			break;
		}
	}
	*cp1 = '\0';
	if (yytext[0] == '-' && yytext[2] == '\0') {
		switch (yytext[1]) {
		case 'b':
			yylval.intval = COMPARE;
			return(OPTION);

		case 'R':
			yylval.intval = REMOVE;
			return(OPTION);

		case 'v':
			yylval.intval = VERIFY;
			return(OPTION);

		case 'w':
			yylval.intval = WHOLE;
			return(OPTION);

		case 'y':
			yylval.intval = YOUNGER;
			return(OPTION);

		case 'h':
			yylval.intval = FOLLOW;
			return(OPTION);

		case 'i':
			yylval.intval = IGNLNKS;
			return(OPTION);
		}
	}
	if (!strcmp(yytext, "install"))
		c = INSTALL;
	else if (!strcmp(yytext, "notify"))
		c = NOTIFY;
	else if (!strcmp(yytext, "except"))
		c = EXCEPT;
	else if (!strcmp(yytext, "except_pat"))
		c = PATTERN;
	else if (!strcmp(yytext, "special"))
		c = SPECIAL;
	else {
		yylval.string = makestr(yytext);
		return(NAME);
	}
	yylval.subcmd = makesubcmd(c);
	return(c);
}

int
any(c, str)
	register int c;
	register char *str;
{
	while (*str)
		if (c == *str++)
			return(1);
	return(0);
}

/*
 * Insert or append ARROW command to list of hosts to be updated.
 */
void
insert(label, files, hosts, subcmds)
	char *label;
	struct namelist *files, *hosts;
	struct subcmd *subcmds;
{
	register struct cmd *c, *prev, *nc;
	register struct namelist *h, *oldh;

	files = expand(files, E_VARS|E_SHELL);
	hosts = expand(hosts, E_ALL);
if (debug) {
	printf("insert:  files = ");
	prnames(files);
	printf("insert:  hosts = ");
	prnames(hosts);
	if (cmds)
		prcmd(cmds);
	else
		printf("insert:  cmds NULL\n");
}
	for (h = hosts; h != NULL; oldh = h, h = h->n_next, free(oldh)) {
		/*
		 * Search command list for an update to the same host.
		 */
		for (prev = NULL, c = cmds; c!=NULL; prev = c, c = c->c_next) {
			if (strcmp(c->c_name, h->n_name) == 0) {
				do {
					prev = c;
					c = c->c_next;
				} while (c != NULL &&
					strcmp(c->c_name, h->n_name) == 0);
				break;
			}
		}
		/*
		 * Insert new command to update host.
		 */
		nc = ALLOC(cmd);
		if (nc == NULL)
			fatal("ran out of memory\n");
		nc->c_type = ARROW;
		nc->c_name = h->n_name;
		nc->c_label = label;
		nc->c_files = files;
		nc->c_cmds = subcmds;
		nc->c_next = c;
		if (prev == NULL)
			cmds = nc;
		else
			prev->c_next = nc;
		/* update last_cmd if appending nc to cmds */
		if (c == NULL)
			last_cmd = nc;
	}
}

/*
 * Append DCOLON command to the end of the command list since these are always
 * executed in the order they appear in the distfile.
 */
static void
append(label, files, stamp, subcmds)
	char *label;
	struct namelist *files;
	char *stamp;
	struct subcmd *subcmds;
{
	register struct cmd *c;

	c = ALLOC(cmd);
	if (c == NULL)
		fatal("ran out of memory\n");
	c->c_type = DCOLON;
	c->c_name = stamp;
	c->c_label = label;
	c->c_files = expand(files, E_ALL);
	c->c_cmds = subcmds;
	c->c_next = NULL;
	if (cmds == NULL)
		cmds = last_cmd = c;
	else {
		last_cmd->c_next = c;
		last_cmd = c;
	}
}

/*
 * Error printing routine in parser.
 */
int
yyerror(const char *s)
{
	extern int yychar;

	nerrs++;
	fflush(stdout);
	fprintf(stderr, "rdist: line %d: %s\n", yylineno, s);
	return (0);
}

/*
 * Return a copy of the string.
 */
char *
makestr(str)
	char *str;
{
	register char *cp, *s;

	str = cp = malloc(strlen(s = str) + 1);
	if (cp == NULL)
		fatal("ran out of memory\n");
	while (*cp++ = *s++)
		;
	return(str);
}

/*
 * Allocate a namelist structure.
 */
struct namelist *
makenl(name)
	char *name;
{
	register struct namelist *nl;

	nl = ALLOC(namelist);
	if (nl == NULL)
		fatal("ran out of memory\n");
	nl->n_name = name;
	nl->n_next = NULL;
	return(nl);
}

/*
 * Duplicate an existing namelist structure.  Only used by the PATTERN
 * code, and then only because expand() is destructive.
 */
struct namelist *
dupnl(old)
	struct namelist *old;
{
	struct namelist *n;
	struct namelist *new, *newhead = (struct namelist *) NULL;
	struct namelist *prev = (struct namelist *) NULL;

	for (n = old; n; n = n->n_next) {
		new = ALLOC(namelist);
		if (new == (struct namelist *) NULL)
			fatal("ran out of memory\n");
		if (newhead == (struct namelist *) NULL)
			newhead = new;
		if (n->n_name) {
			if ((new->n_name = strdup(n->n_name)) == (char *) NULL)
				fatal("ran out of memory\n");
		} else
			new->n_name = (char *) NULL;
		if (prev)
			prev->n_next = new;
		prev = new;
	}
	if (prev)
		prev->n_next = (struct namelist *) NULL;

	return (newhead);
}

/*
 * Make a sub command for lists of variables, commands, etc.
 */
struct subcmd *
makesubcmd(type, name)
	int type;
	register char *name;
{
	register char *cp;
	register struct subcmd *sc;

	sc = ALLOC(subcmd);
	if (sc == NULL)
		fatal("ran out of memory\n");
	sc->sc_type = type;
	sc->sc_args = NULL;
	sc->sc_next = NULL;
	sc->sc_name = NULL;
	return(sc);
}
