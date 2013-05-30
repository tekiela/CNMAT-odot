/*
  Written by John MacCallum, The Center for New Music and Audio Technologies,
  University of California, Berkeley.  Copyright (c) 2009-11, The Regents of
  the University of California (Regents). 
  Permission to use, copy, modify, distribute, and distribute modified versions
  of this software and its documentation without fee and without a signed
  licensing agreement, is hereby granted, provided that the above copyright
  notice, this paragraph and the following two paragraphs appear in all copies,
  modifications, and distributions.

  IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
  SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
  OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
  BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
  HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
  MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.


  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
  NAME: o.message
  DESCRIPTION: Message box for OSC bundles
  AUTHORS: John MacCallum
  COPYRIGHT_YEARS: 2009-11
  SVN_REVISION: $LastChangedRevision: 587 $
  VERSION 0.0: First try
  VERSION 1.0: using updated lib
  VERSION 1.0.1: newlines now delimit messages
  VERSION 2.0: uses newly refactored libo and has initial support for nested bundles
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

*/

#define OMAX_DOC_NAME "o.message"
#define OMAX_DOC_SHORT_DESC "Create and display OSC bundles"
#define OMAX_DOC_LONG_DESC "o.message behaves like the standard Max message box except that it converts its data to OSC packets."
#define OMAX_DOC_INLETS_DESC (char *[]){"Bang sends the OSC FullPacket out.", "Set the contents."}
#define OMAX_DOC_OUTLETS_DESC (char *[]){"OSC FullPacket"}
#define OMAX_DOC_SEEALSO (char *[]){"message"}


#include <string.h>
#include "odot_version.h"


#ifdef OMAX_PD_VERSION
#include "m_pd.h"
#include "g_canvas.h"
#include "g_all_guis.h"
#include "omax_pd_proxy.h"
#define proxy_getinlet(x) (((t_omessage *)(x))->inlet)
#else
#include "ext.h"
#include "ext_obex.h"
#include "ext_obex_util.h"
#include "ext_critical.h"
#include "jpatcher_api.h"
//#include "jpatcher_syms.h"
#include "jgraphics.h"
#endif

#include "omax_util.h"
#include "osc.h"
#include "osc_mem.h"
#include "osc_parser.h"
#include "osc_bundle_u.h"
#include "osc_bundle_s.h"
#include "osc_bundle_iterator_s.h"
#include "osc_bundle_iterator_u.h"
#include "osc_message_iterator_s.h"
#include "osc_message_iterator_u.h"
#include "osc_message_u.h"
#include "osc_message_s.h"
#include "osc_atom_u.h"
#include "osc_atom_s.h"
#include "omax_doc.h"
#include "omax_dict.h"
//#include <mach/mach_time.h>

#include "o.h"

#define OMESSAGE_MAX_NUM_MESSAGES 128
#define OMESSAGE_MAX_MESSAGE_LENGTH 128
#define BUFLEN 128

#ifdef WIN_VERSION
// currently we have to compile windows versions with gcc 3 on cygwin and i'm getting undefined
// refs to strsep, so here it is fucker.
char *
strsep(stringp, delim)
     register char **stringp;
     register const char *delim;
{
	register char *s;
	register const char *spanp;
	register int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}
#endif


enum {
	OMESSAGE_U,
	OMESSAGE_S,
};

#ifdef OMAX_PD_VERSION
typedef struct _omessage {
    t_object    ob;
    t_glist     *glist; //the canvas heirarchy
    
    char        *text;
    char        *hex;
    
    char        *canvas_id;
    char        *handle_id;
    char        *text_id;
    char        *border_tag;
    char        *iolets_tag;
    char        *tcl_namespace;
    
    t_clock     *m_clock;
    
    uint16_t   textediting;
    uint16_t   c_bind;
    
    uint16_t   editmode;
    uint16_t   selected;
    uint16_t   cmdDown;
    
    uint16_t   mouseDown;
    
    t_symbol    *receive_name;
    
    uint32_t    longestline;
    
    uint32_t    width;
    uint32_t    height;

    void *outlet;
	void **proxy;
	long inlet;
    
    void *bndl; // this should be a t_osc_bndl
	int bndltype;
	t_osc_parser_subst *substitutions;
	long nsubs;

	//t_jrgba frame_color, background_color, text_color;
    
} t_omessage;

t_omax_pd_proxy_class *omessage_class;
t_omax_pd_proxy_class *omessage_proxy_class;
t_widgetbehavior omessage_widgetbehavior;

#else
typedef struct _omessage{
	t_jbox ob;
	void *outlet;
	void *proxy;
	long inlet;
	t_critical lock;
	void *bndl; // this should be a t_osc_bndl
	int bndltype;
	t_osc_parser_subst *substitutions;
	long nsubs;
	t_jrgba frame_color, background_color, text_color;
	void *qelem;
} t_omessage;

static t_class *omessage_class;

void omessage_paint(t_omessage *x, t_object *patcherview);
long omessage_key(t_omessage *x, t_object *patcherview, long keycode, long modifiers, long textcharacter);
long omessage_keyfilter(t_omessage *x, t_object *patcherview, long *keycode, long *modifiers, long *textcharacter);
void omessage_mousedown(t_omessage *x, t_object *patcherview, t_pt pt, long modifiers);
void omessage_mouseup(t_omessage *x, t_object *patcherview, t_pt pt, long modifiers);
void omessage_select(t_omessage *x);

#endif

void omessage_doFullPacket(t_omessage *x, long len, char *ptr);
void omessage_set(t_omessage *x, t_symbol *s, long ac, t_atom *av);
void omessage_doselect(t_omessage *x);
void omessage_enter(t_omessage *x);
void omessage_gettext(t_omessage *x);
void omessage_clear(t_omessage *x);
void omessage_output_bundle(t_omessage *x);
void omessage_bang(t_omessage *x);
void omessage_int(t_omessage *x, long n);
void omessage_float(t_omessage *x, double xx);
void omessage_list(t_omessage *x, t_symbol *msg, short argc, t_atom *argv);
void omessage_anything(t_omessage *x, t_symbol *msg, short argc, t_atom *argv);
void omax_util_outletOSC(void *outlet, long len, char *ptr);
void omessage_free(t_omessage *x);
void omessage_inletinfo(t_omessage *x, void *b, long index, char *t);
void *omessage_new(t_symbol *msg, short argc, t_atom *argv);

#ifdef OMAX_PD_VERSION


/* maybe move this to o.h */
#define qelem_set(x)

static int omessage_click(t_gobj *z, struct _glist *glist,
                          int xpix, int ypix, int shift, int alt, int dbl, int doit);
void omessage_insideclick_callback(t_omessage *x);
void omessage_outsideclick_callback(t_omessage *x);
static void omessage_displace(t_gobj *z, t_glist *glist,int dx, int dy);
void omessage_drawElements(t_omessage *x, t_glist *glist, int width2, int height2, int firsttime);
void omessage_storeTextAndExitEditor(t_omessage *x);
void omessage_resetText(t_omessage *x, char *s, int redraw);
void omessage_getTextFromTK(t_omessage *x);

typedef t_omessage t_jbox;
void jbox_redraw(t_jbox *x){ omessage_drawElements((t_omessage *)x, x->glist, x->width, x->height, 0);}

#endif

t_symbol *ps_newline, *ps_FullPacket;

//void omessage_fullPacket(t_omessage *x, long len, long ptr)
void omessage_fullPacket(t_omessage *x, t_symbol *msg, int argc, t_atom *argv)
{
	OMAX_UTIL_GET_LEN_AND_PTR
	if(proxy_getinlet((t_object *)x) == 0){
		return;
	}
	omessage_doFullPacket(x, len, ptr);
}

void omessage_doFullPacket(t_omessage *x, long len, char *ptr){
	osc_bundle_s_wrap_naked_message(len, ptr);
	if(x->bndl){
		switch(x->bndltype){
		case OMESSAGE_U:
			osc_bundle_u_free(x->bndl);
			break;
		case OMESSAGE_S:
			osc_bundle_s_deepFree(x->bndl);
			break;
		}
		x->bndl = NULL;
	}
	if(x->substitutions){
		t_osc_parser_subst *s = x->substitutions;
		while(s){
			t_osc_parser_subst *next = s->next;
			osc_mem_free(s);
			s = next;
		}
		x->substitutions = NULL;
		x->nsubs = 0;
	}
	if(len == OSC_HEADER_SIZE){
		// empty bundle--clear box
		char buf = '\0';
#ifdef OMAX_PD_VERSION
        omessage_resetText(x, &buf, 1);
#else
		object_method(jbox_get_textfield((t_object *)x), gensym("settext"), &buf);
		jbox_redraw((t_jbox *)x);
#endif
		return;
	}

	long bufpos = 0;
	char *buf = NULL;
	int have_subs = 0;
	{
		t_osc_bndl_it_s *bit = osc_bndl_it_s_get(len, (char *)ptr);
		while(osc_bndl_it_s_hasNext(bit) && have_subs == 0){
			t_osc_msg_s *m = osc_bndl_it_s_next(bit);
			char *address = osc_message_s_getAddress(m);
			if(address[1] == '$'){
				have_subs = 1;
				break;
			}
			t_osc_msg_it_s *mit = osc_msg_it_s_get(m);
			while(osc_msg_it_s_hasNext(mit)){
				t_osc_atom_s *a = osc_msg_it_s_next(mit);
				if(osc_atom_s_getTypetag(a) == 's'){
					char *s = NULL;
					osc_atom_s_getString(a, 0, &s);
					if(s){
						if(s[0] == '$'){
							osc_mem_free(s);
							have_subs = 1;
							break;
						}else{
							osc_mem_free(s);
						}
					}
				}
			}
			osc_msg_it_s_destroy(mit);
		}
		osc_bndl_it_s_destroy(bit);
	}

	if(have_subs){
		t_osc_bndl_u *ubndl = NULL;
		osc_bundle_s_deserialize(len, (char *)ptr, &ubndl);
		x->bndl = (void *)ubndl;
		x->bndltype = OMESSAGE_U;
		osc_bundle_u_format(ubndl, &bufpos, &buf);
	}else{
		char *copy = osc_mem_alloc(len);
		memcpy(copy, (char *)ptr, len);
		t_osc_bndl_s *bndl = osc_bundle_s_alloc(len, copy);
		x->bndl = (void *)bndl;
		x->bndltype = OMESSAGE_S;
		osc_bundle_s_format(len, (char *)ptr, &bufpos, &buf);
	}
#ifdef OMAX_PD_VERSION
    omessage_resetText(x, buf, 1);
#else
	object_method(jbox_get_textfield((t_object *)x), gensym("settext"), buf);
#endif

	if(buf){
		osc_mem_free(buf);
	}
	//jbox_redraw((t_jbox *)x);
	qelem_set(x->qelem);
}

void omessage_output_bundle(t_omessage *x)
{
	if(x->bndl){
		switch(x->bndltype){
		case OMESSAGE_U:
			{
				long len = 0;
				char *ptr = NULL;
				osc_bundle_u_serialize((t_osc_bndl_u *)(x->bndl), &len, &ptr);
				omax_util_outletOSC(x->outlet, len, ptr);
				osc_mem_free(ptr);
			}
			break;
		case OMESSAGE_S:
			{
				t_osc_bndl_s *b = (t_osc_bndl_s *)(x->bndl);
				long len = osc_bundle_s_getLen(b);
				char *ptr = osc_bundle_s_getPtr(b);
				char buf[len];
				memcpy(buf, ptr, len);
				omax_util_outletOSC(x->outlet, len, buf);
			}
			break;
		}
	}else{
		char buf[OSC_HEADER_SIZE];
		memset(buf, '\0', OSC_HEADER_SIZE);
		osc_bundle_s_setBundleID(buf);
		omax_util_outletOSC(x->outlet, OSC_HEADER_SIZE, buf);
	}
}

#ifndef OMAX_PD_VERSION
void omessage_paint(t_omessage *x, t_object *patcherview)
{
	t_rect rect;
	t_jgraphics *g = (t_jgraphics *)patcherview_get_jgraphics(patcherview);
	jbox_get_rect_for_view((t_object *)x, patcherview, &rect);

	jgraphics_set_source_jrgba(g, &(x->background_color));
	//jgraphics_rectangle(g, 0., 0., rect.width, rect.height);
	jgraphics_move_to(g, 0, 0);
	jgraphics_line_to(g, 0, rect.height - 8);
	jgraphics_line_to(g, 8, rect.height);
	jgraphics_line_to(g, rect.width, rect.height);
	jgraphics_line_to(g, rect.width, 8);
	jgraphics_line_to(g, rect.width - 8, 0);
	jgraphics_line_to(g, 0, 0);
	jgraphics_fill(g);

	jgraphics_ellipse(g, rect.width - 16., 0., 16, 16);
	jgraphics_ellipse(g, 0., rect.height - 16., 16., 16.);
	jgraphics_fill(g);

	jgraphics_set_source_jrgba(g, &(x->frame_color));
	jgraphics_set_line_width(g, 2.);
	jgraphics_move_to(g, rect.width * .75, 0.);
	jgraphics_line_to(g, 0., 0.);
	jgraphics_line_to(g, 0., rect.height * .25);
	jgraphics_move_to(g, rect.width - (rect.width * .75), rect.height);
	jgraphics_line_to(g, rect.width, rect.height);
	jgraphics_line_to(g, rect.width, rect.height - (rect.height * .25));
	jgraphics_stroke(g);
}

void omessage_refresh(t_omessage *x)
{
	jbox_redraw((t_jbox *)x);
}
#endif

void omessage_processAtoms(t_omessage *x, int argc, t_atom *argv)
{
	if(atom_gettype(argv) != A_SYM){
		error("o.message: not a proper OSC message");
		return;
	}
	if(argc == 1){
		// one arg and it's a symbol.  parse this as it may be an address
		// without arguments, or it may be a complete message like "/foo 1 2 3"

		return;
	}
	// else
	t_symbol *address = atom_getsym(argv);
	if(*(address->s_name) != '/'){
		error("o.message: %s is not a valid OSC address", address->s_name);
		return;
	}

	int len = 2048;
	char *buf = (char *)osc_mem_alloc(2048);
	char *bufptr = buf;
	int i;
	t_osc_bndl_u *bndl = osc_bundle_u_alloc();
	t_osc_msg_u *msg = osc_message_u_alloc();
	osc_message_u_setAddress(msg, address->s_name);
	bufptr += sprintf(bufptr, "%s ", address->s_name);
	t_osc_parser_subst *substitutions = NULL;
	int nsubs = 0;
	for(i = 1; i < argc; i++){
		if(len - (bufptr - buf) < 128){
			int offset = bufptr - buf;
			buf = (char *)osc_mem_resize(buf, len + 1024);
			if(!(buf)){
				error("o.message: out of memory!");
				return;
			}
			len += 1024;
			bufptr = buf + offset;
		}
		switch(atom_gettype(argv + i)){
		case A_LONG:
			{
				int32_t l = atom_getlong(argv + i);
				bufptr += sprintf(bufptr, "%"PRId32" ", l);
				osc_message_u_appendInt32(msg, l);
			}
			break;
		case A_FLOAT:
			{
				float f = atom_getfloat(argv + i);
				bufptr += sprintf(bufptr, "%f", f);
				bufptr--;
				while(*bufptr == '0'){
					*bufptr = ' ';
					*(bufptr + 1) = '\0';
					bufptr--;
				}
				bufptr++;
				*bufptr++ = ' ';
				*bufptr = '\0';
				osc_message_u_appendFloat(msg, f);
			}
			break;
		case A_SYM:
			{
				t_symbol *sym = atom_getsym(argv + i);
				if(sym == ps_newline){
					bufptr += sprintf(bufptr, "%s", sym->s_name);
				}else{
					bufptr += sprintf(bufptr, "%s ", sym->s_name);
				}
				t_osc_atom_u *a = osc_message_u_appendString(msg, sym->s_name);
				if(sym->s_name[0] == '$' && strlen(sym->s_name) > 1){
					t_osc_parser_subst *ss = osc_mem_alloc(sizeof(t_osc_parser_subst));
					ss->msg = msg;
					char *endp = NULL;
					ss->listitem = strtol(sym->s_name + 1, &endp, 0);
					ss->osc_atom = a;
					ss->item_to_replace = osc_message_u_getArgCount(msg);
					ss->next = substitutions;
					substitutions = ss;
					nsubs++;
				}
			}
			break;
		}
	}
#ifdef OMAX_PD_VERSION
    omessage_resetText(x, buf, 1);
#else
	object_method(jbox_get_textfield((t_object *)x), gensym("settext"), buf);
#endif

	if(buf){
		osc_mem_free(buf);
	}
	osc_bundle_u_addMsg(bndl, msg);

	if(x->bndl){
		switch(x->bndltype){
		case OMESSAGE_U:
			osc_bundle_u_free(x->bndl);
			break;
		case OMESSAGE_S:
			osc_bundle_s_free(x->bndl);
			char *ptr = osc_bundle_s_getPtr(x->bndl);
			if(ptr){
				osc_mem_free(ptr);
			}
			break;
		}
		x->bndl = NULL;
	}
	t_osc_parser_subst *s = x->substitutions;
	while(s){
		t_osc_parser_subst *next = s->next;
		osc_mem_free(s);
		s = next;
	}
	x->substitutions = NULL;
	x->nsubs = 0;
	if(substitutions){
		x->substitutions = substitutions;
		x->nsubs = nsubs;
		x->bndl = bndl;
		x->bndltype = OMESSAGE_U;
	}else{
		long len = 0;
		char *bndl_s = NULL;
		osc_bundle_u_serialize(bndl, &len, &bndl_s);
		osc_bundle_u_free(bndl);
		x->bndl = osc_bundle_s_alloc(len, bndl_s);
		x->bndltype = OMESSAGE_S;
	}
}

#ifndef OMAX_PD_VERSION
void omessage_select(t_omessage *x){
	defer(x, (method)omessage_doselect, 0, 0, 0);
}

void omessage_doselect(t_omessage *x){
	t_object *p = NULL; 
	object_obex_lookup(x,gensym("#P"), &p);
	if (p) {
		t_atom rv; 
		long ac = 1; 
		t_atom av[1]; 
		atom_setobj(av, x); 
		object_method_typed(p, gensym("selectbox"), ac, av, &rv); 
	}
}


long omessage_key(t_omessage *x, t_object *patcherview, long keycode, long modifiers, long textcharacter){
	char buff[256];
	buff[0] = textcharacter;  // we know this is just a simple char
	buff[1] = 0; 
	object_method(patcherview, gensym("insertboxtext"), x, buff);
	jbox_redraw((t_jbox *)x);

	return 1; 
}

long omessage_keyfilter(t_omessage *x, t_object *patcherview, long *keycode, long *modifiers, long *textcharacter){
	t_atom arv;
	long rv = 1;
	long k = *keycode;
	
	if (k == JKEY_TAB || k == JKEY_ESC) {
		object_method_typed(patcherview, gensym("endeditbox"), 0, NULL, &arv); 
		rv = 0;		// don't pass those keys to omessage
	}
	return rv;
}


void omessage_mousedown(t_omessage *x, t_object *patcherview, t_pt pt, long modifiers){
    textfield_set_textmargins(jbox_get_textfield((t_object *)x), 4, 4, 2, 2);
	jbox_redraw((t_jbox *)x);
}

void omessage_mouseup(t_omessage *x, t_object *patcherview, t_pt pt, long modifiers){
    textfield_set_textmargins(jbox_get_textfield((t_object *)x), 3, 3, 3, 3);
	jbox_redraw((t_jbox *)x);
	omessage_output_bundle(x);
}
#endif

// enter is triggerd at "endeditbox time"
void omessage_enter(t_omessage *x){
	omessage_gettext(x);
}

void omessage_gettext(t_omessage *x){
	long size	= 0;
	char *text	= NULL;
#ifdef OMAX_PD_VERSION
    omessage_getTextFromTK(x);
    text = x->text;
#else
	t_object *textfield = jbox_get_textfield((t_object *)x);
	object_method(textfield, gensym("gettextptr"), &text, &size);
#endif
    
	{
		if(x->bndl){
			switch(x->bndltype){
			case OMESSAGE_U:
				osc_bundle_u_free(x->bndl);
				break;
			case OMESSAGE_S:
				osc_bundle_s_deepFree(x->bndl);
				break;
			}
			x->bndl = NULL;
		}


		size = strlen(text); // the value returned in text doesn't make sense
		if(size == 0){
			if(x->bndl){
				switch(x->bndltype){
				case OMESSAGE_U:
					osc_bundle_u_free(x->bndl);
					break;
				case OMESSAGE_S:
					{
						char *p = osc_bundle_s_getPtr(x->bndl);
						if(p){
							osc_mem_free(p);
						}
						osc_bundle_s_free(x->bndl);
					}
					break;
				}
				x->bndl = NULL;
			}
			return;
		}
		char *buf = text;

		if(text[size - 1] != '\n'){
			buf = alloca(size + 2);
			memcpy(buf, text, size);
			buf[size] = '\n';
			buf[size + 1] = '\0';
			size += 2;
		}

		t_osc_bndl_u *bndl = NULL;
		t_osc_parser_subst *subs = NULL;
		long nsubs = 0;
		t_osc_err e = osc_parser_parseString(size, buf, &bndl, &nsubs, &subs);
		if(e){
			object_error((t_object *)x, "error parsing bundle\n");
			return;
		}
		// format parsed bundle and display it
		char *formatted = NULL;
		long formattedlen = 0;
		osc_bundle_u_format(bndl, &formattedlen, &formatted);
		char *ptr = formatted + (formattedlen - 1);
		while(*ptr == '\n'){
			*ptr = '\0';
			ptr--;
		}
        
#ifdef OMAX_PD_VERSION
        omessage_resetText(x, formatted, 1);
#else
		object_method(jbox_get_textfield((t_object *)x), gensym("settext"), formatted);
#endif
        
        if(formatted){
			osc_mem_free(formatted);
		}
		if(subs){
			x->bndl = bndl;
			x->bndltype = OMESSAGE_U;
		}else{
			long len = 0;
			char *buf = NULL;
			osc_bundle_u_serialize(bndl, &len, &buf);
			t_osc_bndl_s *b = osc_bundle_s_alloc(len, buf);
			x->bndl = (void *)b;
			x->bndltype = OMESSAGE_S;
			osc_bundle_u_free(bndl);
		}
		if(x->substitutions){
			t_osc_parser_subst *s = x->substitutions;
			while(s){
				t_osc_parser_subst *next = s->next;
				osc_mem_free(s);
				s = next;
			}
			x->substitutions = NULL;
			x->nsubs = 0;
		}
		x->substitutions = subs;
		x->nsubs = nsubs;
	}
	if(size > 2){
		int i;
		char *r = text + 1;
		char *w = text + 1;
		char *rm1 = text;
		for(i = 0; i <= size; i++){
			if(*rm1 == ' ' && *r == ' '){
				r++;
			}else{
				*w++ = *r++;
			}
			rm1++;
		}
	}
}

void omessage_bang(t_omessage *x){
	omessage_output_bundle(x);
}

void omessage_int(t_omessage *x, long n){
	t_atom a;
	atom_setlong(&a, n);
	omessage_list(x, NULL, 1, &a);
}

void omessage_float(t_omessage *x, double f){
	t_atom a;
	atom_setfloat(&a, f);
	omessage_list(x, NULL, 1, &a);
}

void omessage_list(t_omessage *x, t_symbol *list_sym, short argc, t_atom *argv){
	switch(proxy_getinlet((t_object *)x)){
	case 0:
		{
			if(x->bndltype == OMESSAGE_S){
				// this is lame...  we can't just deserialize because that process 
				// doesn't produce the $n substitution structure.  so we have 
				// to get the text and parse it.  this is the right place to do it
				// rather than the fullpacket function because we don't know if 
				// we'll have to do the substitutions or not.
				/*
				t_osc_bndl_u *bndl = NULL;
				osc_bundle_s_deserialize(osc_bundle_s_getLen((t_osc_bndl_s *)x->bndl),
							 osc_bundle_s_getPtr((t_osc_bndl_s *)x->bndl),
							 &bndl);
				if(!bndl){
					object_error((t_object *)x, "couldn't deserialize bundle!");
					return;
				}
				x->bndl = bndl;
				x->bndltype = OMESSAGE_U;
				*/
				omessage_gettext(x);
				jbox_redraw((t_jbox *)x);
			}
			if(x->nsubs == 0 || x->bndltype == OMESSAGE_S){
				object_error((t_object *)x, "can't find $n variables to substitute");
				return;
			}

			char *original_addresses[x->nsubs];
			t_osc_msg_u *msgs[x->nsubs];
			long nsubs = 0;
			memset(original_addresses, '\0', x->nsubs);

			// process address
			t_osc_bndl_it_u *it = osc_bndl_it_u_get(x->bndl);
			while(osc_bndl_it_u_hasNext(it)){
				t_osc_msg_u *msg = osc_bndl_it_u_next(it);
				char *address = osc_message_u_getAddress(msg);
				int addresslen = strlen(address) + 1;
				char copy[addresslen];
				char *copyptr = copy;
				memcpy(copy, address, addresslen);
				int newaddresslen = addresslen;
				int dosub = 0;
				int addedtolist = 0;
				while(strsep(&copyptr, "$")){
					if(copyptr){
						if(addedtolist == 0){
							original_addresses[nsubs] = address;
							msgs[nsubs] = msg;
							nsubs++;
							addedtolist = 1;
						}
						dosub = 1;
						char *endp = NULL;
						long l = strtol(copyptr, &endp, 0) - 1;
						if(l < argc){
							switch(atom_gettype(argv + l)){
							case A_LONG:
								newaddresslen += snprintf(NULL, 0, "%lld", atom_getlong(argv + l));
								break;
							case A_FLOAT:
								newaddresslen += snprintf(NULL, 0, "%f", atom_getfloat(argv + l));
								break;
							case A_SYM:
								newaddresslen += strlen(atom_getsym(argv + l)->s_name);
								break;
							}
						}
					}
				}
				if(dosub){

					newaddresslen += 16; // never can be too careful...
					char *newaddress = osc_mem_alloc(newaddresslen);
					char *ptr = newaddress;
					memcpy(copy, address, addresslen);
					copyptr = copy;
					char *lasttok = copy, *tok = NULL;
					while((tok = strsep(&copyptr, "$"))){
						ptr += sprintf(ptr, "%s", lasttok);
						if(copyptr){
							dosub = 1;
							char *endp = NULL;
							long l = strtol(copyptr, &endp, 0) - 1;
							if(l < argc){
								switch(atom_gettype(argv + l)){
								case A_LONG:
									ptr += sprintf(ptr, "%ld", atom_getlong(argv + l));
									break;
								case A_FLOAT:
									ptr += sprintf(ptr, "%f", atom_getfloat(argv + l));
									break;
								case A_SYM:
									ptr += sprintf(ptr, "%s", atom_getsym(argv + l)->s_name);
									break;
								}
							}
							lasttok = endp;
						}
					}
					osc_message_u_setAddressPtr(msg, newaddress, NULL);
				}
			}
			osc_bndl_it_u_destroy(it);

			// now do argument substitutions
			if(x->substitutions){
				if(x->bndltype == OMESSAGE_U){
					t_osc_parser_subst *s = x->substitutions;
					while(s){
						if(s->listitem > argc){
							s = s->next;
							continue;
						}
						t_atom *maxatom = argv + s->listitem - 1;

						if(s->item_to_replace == 0){
							// this shouldn't happen
						}else{
							t_osc_atom_u *a = s->osc_atom;
							switch(atom_gettype(maxatom)){
							case A_FLOAT:
								osc_atom_u_setFloat(a, atom_getfloat(maxatom));
								break;
							case A_LONG:
								osc_atom_u_setInt32(a, atom_getlong(maxatom));
								break;
							case A_SYM:
								osc_atom_u_setString(a, atom_getsym(maxatom)->s_name);
								break;
							}
						}
						s = s->next;
					}
				}
			}

			long len = 0;
			char *buf = NULL;
			osc_bundle_u_serialize(x->bndl, &len, &buf);
			omax_util_outletOSC(x->outlet, len, buf);
			osc_mem_free(buf);
			t_osc_parser_subst *s = x->substitutions;
			while(s){
				if(s->listitem > argc){
					s = s->next;
					continue;
				}
				t_osc_atom_u *a = s->osc_atom;
				if(s->item_to_replace == 0){

				}else{
					char buf[8];
					sprintf(buf, "$%d", s->listitem);
					osc_atom_u_setString(a, buf);
				}
				s = s->next;
			}
			int i;
			for(i = 0; i < nsubs; i++){
				char *ptr = NULL;
				osc_message_u_setAddressPtr(msgs[i], original_addresses[i], &ptr);
				if(ptr){
					osc_mem_free(ptr);
				}
			}
		}
		break;
	case 1:

		break;
	}
}

void omessage_anything(t_omessage *x, t_symbol *msg, short argc, t_atom *argv)
{
	t_atom av[argc + 1];
	int ac = argc;

	if(msg){
		ac = argc + 1;
		atom_setsym(av, msg);
		if(argc > 0){
			memcpy(av + 1, argv, argc * sizeof(t_atom));
		}
	}else{
		memcpy(av, argv, argc * sizeof(t_atom));
	}
	switch(proxy_getinlet((t_object *)x)){
	case 0:
		omessage_list(x, NULL, ac, av);
		break;
	case 1:
		omessage_processAtoms(x, ac, av);
		break;
	}
	jbox_redraw((t_jbox *)x);
}

void omessage_set(t_omessage *x, t_symbol *s, long ac, t_atom *av)
{
	if(proxy_getinlet((t_object *)x)){
		return;
	}
	if(ac){
		if(atom_gettype(av) == A_SYM){
			t_symbol *sym = atom_getsym(av);
			if(sym == ps_FullPacket && ac == 3){
				omessage_doFullPacket(x, atom_getlong(av + 1), (char *)atom_getlong(av + 2));
				return;
			}
			omessage_processAtoms(x, ac, av);
		}
	}else{
		omessage_clear(x);
	}
	jbox_redraw((t_jbox *)x);
}

void omessage_clear(t_omessage *x)
{
	char buf[OSC_HEADER_SIZE];
	memset(buf, '\0', OSC_HEADER_SIZE);
	osc_bundle_s_setBundleID(buf);
	omessage_doFullPacket(x, OSC_HEADER_SIZE, buf);
}
/*
    ...........................................................................................
    ................................  PD VERSION  .............................................
    ...........................................................................................
 */
#ifdef OMAX_PD_VERSION


static void omessage_getrect(t_gobj *z, t_glist *glist,int *xp1, int *yp1, int *xp2, int *yp2)
{
    //   post("%s", __func__);
    t_omessage *x = (t_omessage *)z;
    int x1, y1, x2, y2;
    
    //do this in the text editing part maybe?  or at least coordinate them better
    /*
     int font = glist_getfont(glist);
     int fontwidth = sys_fontwidth(font), fontheight = sys_fontheight(font);
     x->width = (x->width > 0 ? x->width : 6) * fontwidth + 2;
     x->height = fontheight + 3;
     */
    x1 = text_xpix(&x->ob, glist);
    y1 = text_ypix(&x->ob, glist);
    x2 = x1 + x->width;
    y2 = y1 + x->height;
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}


void omessage_bind_text_events(t_omessage *x)
{
    //could add bindings for cmd-1, 2, etc. to deselect when you make a new
    
    post("%s", __func__);
    
    sys_vgui("bind %s <Key> {+pdsend {%s key %%N }}\n",    x->text_id, x->receive_name->s_name);
    sys_vgui("bind %s <KeyRelease> {+pdsend {%s keyup %%N }}\n",    x->text_id, x->receive_name->s_name);
    
    sys_vgui("namespace eval ::%s [list set canvas%lxBUTTONBINDING [bind %s <Button-1>]] \n", x->tcl_namespace, glist_getcanvas(x->glist), x->canvas_id);
    sys_vgui("namespace eval ::%s [list set canvas%lxKEYBINDING [bind %s <Key>]] \n", x->tcl_namespace, glist_getcanvas(x->glist), x->canvas_id);
    
    //focusout for clicking to other windows other than the main canvas
    sys_vgui("bind %s <FocusOut> {+pdsend {%s pdnofocus }}\n", x->text_id, x->receive_name->s_name);
    
    if(!x->c_bind)
    {
        sys_vgui("bind %s <Button-1> {+pdsend {%s outsideclick }}\n", x->canvas_id, x->receive_name->s_name);
        x->c_bind = 1;
    }
    
}

void omessage_getTextFromTK(t_omessage *x)
{
    if(x->textediting){
        omessage_storeTextAndExitEditor(x);
    } else {
        sys_vgui("pdsend \"%s textbuf hex [string2hex $::%s::textbuf%lx] \"\n", x->receive_name->s_name, x->tcl_namespace, glist_getcanvas(x->glist));
    }
}

void omessage_storeTextAndExitEditor(t_omessage *x)
{
    if(x->textediting){
        t_canvas *c = glist_getcanvas(x->glist);
        
        sys_vgui("pdsend \"%s textbuf hex [string2hex [%s get 0.0 end]] \"\n", x->receive_name->s_name, x->text_id);
        
        //might not need to save the string in tcl since we will save it in odot, leaving in for now
        sys_vgui("namespace eval ::%s [list set textbuf%lx [string trimright [%s get 0.0 end]]] \n", x->tcl_namespace, c, x->text_id, x->text_id);
        
        //destroy editor and print text to canvas
        sys_vgui("destroy %s\n", x->text_id);
        sys_vgui("%s create text %d %d -anchor nw -width %d -font {{%s} %d %s} -tags text%lx -text $::%s::textbuf%lx\n", x->canvas_id, text_xpix(&x->ob, x->glist)+5, text_ypix(&x->ob, x->glist)+5, x->width-10, sys_font, glist_getfont(x->glist), sys_fontweight, (long)x, x->tcl_namespace, c );
        
        x->textediting = 0;
    }
    
}

void omessage_getTextAndCreateEditor(t_omessage *x, int firsttime)
{
    int x1 = text_xpix(&x->ob, x->glist);
    int y1 = text_ypix(&x->ob, x->glist);
    
    if(firsttime)
    {
        sys_vgui("%s delete text%lx\n", x->canvas_id, (long)x);
        glist_noselect(x->glist);
        sys_vgui("text %s -font {{%s} %d %s} -undo true -fg \"black\" -bg $msg_box_fill -takefocus 1 -state normal -highlightthickness 0\n", x->text_id, sys_font, glist_getfont(x->glist), sys_fontweight );
        sys_vgui("place %s -x %d -y %d -width %d -height %d\n", x->text_id, x1+4, y1+4, x->width-8, x->height-8);
        
        sys_vgui("%s insert 1.0 $::%s::textbuf%lx \n", x->text_id, x->tcl_namespace, glist_getcanvas(x->glist));
        sys_vgui("event generate %s <1> -x %d -y %d \n", x->text_id, x1 + 10, y1 + 5);
        sys_vgui("event generate %s <ButtonRelease-1> -x %d -y %d \n", x->text_id, x1 + 10, y1 + 5);
        sys_vgui("%s tag add sel 0.0 end\n", x->text_id);
        
        omessage_bind_text_events(x);
    }
    else
    {
        post("%s not first time", __func__);
        sys_vgui("place %s -x %d -y %d -width %d -height %d\n", x->text_id, x1+5, y1+5, x->width-10, x->height-10);
    }
    
    {
        //this should be somewhere else
        x->editmode = glist_getcanvas(x->glist)->gl_edit;
        if(x->editmode && !x->selected)
        {
            sys_vgui("%s configure -cursor hand2\n", x->text_id);
        } else if(x->editmode && x->selected) {
            sys_vgui("%s configure -cursor xterm\n", x->text_id);
        } else if(!x->editmode){
            sys_vgui("%s configure -cursor center_ptr\n", x->text_id);
        }
    }
    
    x->textediting = 1;
    
    //sys_vgui("pdsend \"%s textbuf $::%s::textbuf%lx \"\n", x->receive_name->s_name, x->tcl_namespace, glist_getcanvas(x->glist));
    
}

void omessage_resetText(t_omessage *x, char *s, int redraw)
{
    if(s)
        post("%s %s", __func__,s);
    
    //[string map { \" ' } \"%s\"]
    sys_vgui("namespace eval ::%s [list set textbuf%lx [string map -nocase {\\\" \' } {%s} ] ]\n", x->tcl_namespace, glist_getcanvas(x->glist), s);
    omessage_getTextFromTK(x);
    if(redraw)
        omessage_drawElements(x, x->glist, x->width, x->height, 0);
}


void omessage_drawElements(t_omessage *x, t_glist *glist, int width2, int height2, int firsttime)
{
    int x1, y1, x2, y2;
    omessage_getrect((t_gobj *)x, glist, &x1, &y1, &x2, &y2);
    
    t_canvas *canvas = glist_getcanvas(glist);
    //int msg_draw_const = ((y2-y1)/4);
    //if (msg_draw_const > 10) msg_draw_const = 10; /* looks bad if too big */
    if (firsttime)
    {
        sys_vgui("%s create rectangle %d %d %d %d -outline $box_outline -fill $msg_box_fill -tags [list %s msg]\n",x->canvas_id, x1, y1, x2, y2, x->border_tag);
        
        sys_vgui("canvas %s -width 5 -height 5  -bg $box_outline -cursor fleur \n", x->handle_id);
        
        sys_vgui("place %s -x %d -y %d -width %d -height %d\n", x->handle_id, x2-5, y2-5, 5, 5);
        
        sys_vgui("bind %s <Button-1> {+pdsend {%s resize_mousedown}} \n", x->handle_id, x->receive_name->s_name );
        sys_vgui("bind %s <Motion> {+pdsend {%s resize_mousemove %%x %%y }} \n", x->handle_id, x->receive_name->s_name );
        sys_vgui("bind %s <ButtonRelease-1> {+pdsend {%s resize_mouseup }} \n", x->handle_id, x->receive_name->s_name );
        
        
        sys_vgui("%s create text %d %d -anchor nw -width %d -font {{%s} %d %s} -tags text%lx -text $::%s::textbuf%lx\n", x->canvas_id, text_xpix(&x->ob, x->glist)+5, text_ypix(&x->ob, x->glist)+5, x->width-10, sys_font, glist_getfont(x->glist), sys_fontweight, (long)x, x->tcl_namespace, canvas );
    }
    else
    {
        sys_vgui(".x%lx.c coords %s %d %d %d %d\n", canvas, x->border_tag, x1, y1, x2, y2);
        sys_vgui("place %s -x %d -y %d -width %d -height %d\n", x->handle_id, x2-5, y2-5, 5, 5);
        if(x->textediting)
        {
            sys_vgui("place %s -x %d -y %d -width %d -height %d\n", x->text_id, x1+5, y1+5, x->width-10, x->height-10);
        }
        else
        {
            sys_vgui("%s itemconfigure text%lx -width %d -text $::%s::textbuf%lx\n", x->canvas_id, (long)x, x->width-10, x->tcl_namespace, canvas);
        }
    }
    
    /* draw inlets/outlets */
    t_object *ob = pd_checkobject(&x->ob.te_pd);
    if (ob){
        glist_drawiofor(glist, ob, firsttime, x->iolets_tag, x1, y1, x2, y2);
    }
    if (firsttime) /* raise cords over everything else */
        sys_vgui(".x%lx.c raise cord\n", glist_getcanvas(glist));
    
    
}

void omessage_resize_mousedown(t_omessage *x)
{
    post("%s", __func__);
    x->mouseDown = 1;
}

void omessage_resize_mousemove(t_omessage *x, float dx, float dy)
{
    //    post("%s %f %f", __func__, xx, yy);
    if(x->mouseDown)
    {
        x->width += (int)dx;
        
        x->height += (int)dy;
        
        omessage_drawElements(x, x->glist, x->width, x->height, 0);
        
    }
}

void omessage_resize_mouseup(t_omessage *x)
{
    post("%s",__func__);
    x->mouseDown = 0;
    sys_vgui("focus %s\n", x->canvas_id);
}


/*
void omessage_bang(t_omessage *x)
{
    omessage_storeTextAndExitEditor(x);
    //    sys_vgui("pdsend \"%s textbuf [%s get 0.0 end]\"\n", x->receive_name->s_name, x->text_id);
}
*/

//http://stackoverflow.com/questions/5403103/hex-to-ascii-string-conversion
int hex_to_int(char c){
    if(c >=97)
        c=c-32;
    int first = c / 16 - 3;
    int second = c % 16;
    int result = first*10 + second;
    if(result > 9) result--;
    return result;
}

int hex_to_ascii(char c, char d){
    int high = hex_to_int(c) * 16;
    int low = hex_to_int(d);
    return high+low;
}

void omessage_textbuf(t_omessage *x, t_symbol *msg, int argc, t_atom *argv)
{
    
    if(msg)
        post("%s %s %d", __func__, msg->s_name, argc);
    
    if(argc == 2)
    {
        if(argv->a_type == A_SYMBOL && atom_getsymbol(argv) == gensym("hex"))
        {
            char *st = atom_getsymbol(argv+1)->s_name;
            int hexlen = strlen(st);
            int length = hexlen / 2;
            
            x->hex = (char *)realloc(x->hex, sizeof(char) * hexlen);
            x->hex = st;
            
            x->text = (char *)realloc(x->text, sizeof(char) * length);
            
            int j, k;
            int c;
            for(j = 0, k = 0; j < length; j++, k=j*2){
                c = hex_to_ascii(st[k], st[k+1]);
                /*
                 post("%d %x %c %d", j, c, c, c);
                 if (c == 0x0a){
                 post("new line");
                 }
                 */
                x->text[j] = (char)c;
                
            }
            x->text[length] = 0; //<< not sure if this is necessary
            
            post("%s %s", __func__, x->text);
            
        }
        else
        {
            //error
        }
    }
    /*
     
     int i;
     for(i = 0; i < argc; i++)
     {
     if((argv+i)->a_type == A_SYMBOL){
     
     
     }
     else if((argv+i)->a_type == A_FLOAT){
     float f = atom_getfloat(argv+i);
     post("%s f %f", __func__, f);
     }
     
     }
     */
    
}


void omessage_insideclick_callback(t_omessage *x)
{
    post("%s", __func__);
    t_canvas *canvas = glist_getcanvas(x->glist);
    if(canvas->gl_edit)
    {
        //this might be "activate" versus "select"
        //activate is text edit mode and select is move or delete mode (resize too)
        
        sys_vgui("focus %s\n", x->text_id);
        glist_noselect(x->glist);
        gobj_select((t_gobj *)x, x->glist, 1);
        if(!x->c_bind)
        {
            sys_vgui("bind %s <Button-1> {+pdsend {%s outsideclick }}\n", x->canvas_id, x->receive_name->s_name);
            x->c_bind = 1;
        }
    } else {
        sys_vgui("focus %s\n", x->canvas_id);
        omessage_storeTextAndExitEditor(x);
        omessage_click((t_gobj *)x, x->glist, 0, 0, 0, 0, 0, 0);
    }
    
}

void omessage_outsideclick_callback(t_omessage *x)
{
    post("%s", __func__);
    sys_vgui("focus %s\n", x->canvas_id);
    gobj_select((t_gobj *)x, x->glist, 0);
    sys_vgui("bind %s <Button-1> $::%s::canvas%lxBUTTONBINDING\n", x->canvas_id, x->tcl_namespace, glist_getcanvas(x->glist));
    
    omessage_storeTextAndExitEditor(x);
    //same for <Key>
    x->selected = 0;
    x->c_bind = 0;
}


//called when clicking from one object to another without clicking on the empty canvas first
void omessage_pdnofocus_callback(t_omessage *x)
{
    post("%s", __func__);
    //  gobj_select((t_gobj *)x, x->glist, 0);
    sys_vgui("bind %s <Button-1> $::%s::canvas%lxBUTTONBINDING\n", x->canvas_id, x->tcl_namespace, glist_getcanvas(x->glist));
    x->c_bind = 0;
}

void omessage_setWidth(t_omessage *x, int w)
{
    x->width = w;
    //resize box now
}

void omessage_keyup_callback(t_omessage *x, t_symbol *s, int argc, t_atom *argv)
{
    if(s)
        post("%s %s argc %d", __func__, s->s_name, argc);
    
    if(argc == 1)
    {
        if(argv->a_type == A_FLOAT)
        {
            int k = (int)atom_getfloat(argv);
            if(k == 65511)
            {
                x->cmdDown = 0;
            }
            
        }
    }
    
    
}

void omessage_key_callback(t_omessage *x, t_symbol *s, int argc, t_atom *argv)
{
    if(s)
        post("%s %s argc %d", __func__, s->s_name, argc);
    
    if(argc == 1)
    {
        if(argv->a_type == A_FLOAT)
        {
            post("%s %d", __func__, (int)atom_getfloat(argv));
            int k = (int)atom_getfloat(argv);
            switch (k) {
                case 65307: //esc
                    omessage_outsideclick_callback(x);
                    return;
                    break;
                case 65511:
                    x->cmdDown = 1;
                    break;
                default:
                    break;
            }
            
            if(x->cmdDown){
                if(k >= 49 && k <= 53)
                {
                    omessage_outsideclick_callback(x);
                    return;
                }
                else
                {
                    switch (k) {
                        case 66:
                        case 84:
                        case 78:
                        case 86:
                        case 72:
                        case 68:
                        case 73:
                        case 85:
                        case 67:
                            omessage_outsideclick_callback(x);
                            break;
                        default:
                            break;
                    }
                }
            }
            
            
        }
        
        if(argv->a_type == A_SYMBOL)
        {
            post("%s %c", __func__, (char)atom_getsymbol(argv));
        }
    }
    
}

static void omessage_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_omessage *x = (t_omessage *)z;
    
    if(vis)
    {
        omessage_drawElements(x, glist, x->width, x->height, 1);
        if(x->textediting)
        {
            omessage_getTextAndCreateEditor(x, 1);
        }
    }
}

static void omessage_displace(t_gobj *z, t_glist *glist,int dx, int dy)
{
    t_omessage *x = (t_omessage *)z;
    if(glist_isvisible(glist))
    {
        t_canvas *canv = glist_getcanvas(glist);
        x->ob.te_xpix += dx;
        x->ob.te_xpix = (x->ob.te_xpix >= 0) ? x->ob.te_xpix : 0;
        x->ob.te_xpix = (x->ob.te_xpix + x->width) <= canv->gl_screenx2 ? x->ob.te_xpix : x->ob.te_xpix - x->width;
        
        x->ob.te_ypix += dy;
        x->ob.te_ypix = (x->ob.te_ypix >= 0) ? x->ob.te_ypix : 0;
        x->ob.te_ypix = (x->ob.te_ypix + x->height) <= canv->gl_screeny2 ? x->ob.te_ypix : x->ob.te_ypix - x->height;
        
        int x2 = x->ob.te_xpix+x->width;
        int y2 = x->ob.te_ypix+x->height;
        
        sys_vgui("%s coords %s %d %d %d %d\n", x->canvas_id, x->border_tag, x->ob.te_xpix, x->ob.te_ypix, x2, y2);
        sys_vgui("place %s -x %d -y %d -width %d -height %d\n", x->handle_id, x2-5, y2-5, 5, 5);
        
        omessage_drawElements(x, glist, x->width, x->height, 0);
        
        if(x->textediting)
        {
            sys_vgui("place %s -x %d -y %d -width %d -height %d\n", x->text_id, x->ob.te_xpix+5, x->ob.te_ypix+5, x->width-10, x->height-10);
            
        }
        else
        {
            sys_vgui("%s move text%lx %d %d\n", x->canvas_id, (long)x, dx, dy);
        }
        
        canvas_fixlinesfor(glist, &x->ob);
    }
}

static void omessage_select(t_gobj *z, t_glist *glist, int state)
{
    post("%s %d", __func__, state);
    t_omessage *x = (t_omessage *)z;
    if(state && !x->selected)
    {
        //        sys_vgui("%s configure -state disabled\n",x->text_id);
        x->selected = 1;
    }
    else if(state && x->selected)
    {
        if(!x->textediting)
        {
            //          omessage_getTextAndCreateEditor(x, 1);
            //            sys_vgui("%s configure -state normal\n", x->text_id);
        }
    }
    else
    {
        //        sys_vgui("%s configure -state disabled\n",x->text_id );
        omessage_storeTextAndExitEditor(x);
        x->selected = 0;
        
        //  sys_vgui("%s configure -state disabled -cursor $cursor_editmode_nothing\n",x->text_id);
    }
    
    if (glist_isvisible(glist) && gobj_shouldvis(&x->ob.te_g, glist)){
        sys_vgui(".x%lx.c itemconfigure %s -outline %s\n", glist, x->border_tag, (state? "$select_color" : "$box_outline"));
        if(!x->textediting)
            sys_vgui(".x%lx.c itemconfigure text%lx -fill %s\n", glist, (long)x, (state? "$select_color" : "black"));
    }
}

static void omessage_activate(t_gobj *z, t_glist *glist, int state)
{
    post("%s %d", __func__, state);
    
    t_omessage *x = (t_omessage *)z;
    if(state)
    {
        omessage_getTextAndCreateEditor(x, 1);
    }
    else
    {
        omessage_storeTextAndExitEditor(x);
        omessage_gettext(x);
    }
    
    sys_vgui(".x%lx.c itemconfigure %s -outline %s\n", glist, x->border_tag, (state? "$select_color" : "$box_outline"));
    
}

static void omessage_delete(t_gobj *z, t_glist *glist)
{
    post("%s", __func__);
    t_omessage *x = (t_omessage *)z;
    
    glist_eraseiofor(glist, &x->ob, x->iolets_tag);
    
    sys_vgui("%s delete %s\n", x->canvas_id, x->border_tag);
    sys_vgui("%s delete text%lx \n", x->canvas_id, (long)x);
    if(x->textediting)
        sys_vgui("destroy %s\n", x->text_id);
    
    sys_vgui("destroy %s\n", x->handle_id);
    
    
}


static void omessage_doClick(t_omessage *x,
                             t_floatarg xpos, t_floatarg ypos, t_floatarg shift,
                             t_floatarg ctrl, t_floatarg alt)
{
    if (glist_isvisible(x->glist))
    {
        sys_vgui(".x%lx.c itemconfigure %s -width 5\n", glist_getcanvas(x->glist), x->border_tag);
        omessage_bang(x);
        clock_delay(x->m_clock, 120);
    }
}


static int omessage_click(t_gobj *z, struct _glist *glist,
                          int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
//    post("%s %d", __func__, doit);
    
    t_text *x = (t_text *)z;
    {
        if(doit)
        {
            omessage_doClick((t_omessage *)x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, (t_floatarg)0, (t_floatarg)alt);
        }
        return (1);
        
    }
}


static void omessage_tick(t_omessage *x)
{
    if (glist_isvisible(x->glist))
    {
        sys_vgui(".x%lx.c itemconfigure %s -width 1\n",
                 glist_getcanvas(x->glist), x->border_tag);
    }
}


static void omessage_save(t_gobj *z, t_binbuf *b)
{
    
    t_omessage *x = (t_omessage *)z;
    binbuf_addv(b, "ssiisiiss;", gensym("#X"),gensym("obj"),(t_int)x->ob.te_xpix, (t_int)x->ob.te_ypix, gensym("o_message"), x->width, x->height, gensym("hex"), gensym(x->hex));
    
}



void omessage_free(t_omessage *x)
{
    free(x->text);
    free(x->hex);
    free(x->text_id);
    free(x->canvas_id);
    free(x->tcl_namespace);
    free(x->border_tag);
    free(x->iolets_tag);
    free(x->handle_id);
    pd_unbind(&x->ob.ob_pd, x->receive_name);
    clock_free(x->m_clock);
    

    {
    
        critical_free(x->lock);
        if(x->proxy){
            free(x->proxy);
        }
        if(x->bndl){
            switch(x->bndltype){
                case OMESSAGE_S:
                    osc_bundle_s_deepFree((t_osc_bndl_s *)x->bndl);
                    break;
                case OMESSAGE_U:
                    osc_bundle_u_free((t_osc_bndl_u *)x->bndl);
                    break;
            }
        }
        if(x->substitutions){
            t_osc_parser_subst *s = x->substitutions;
            while(s){
                t_osc_parser_subst *next = s->next;
                osc_mem_free(s);
                s = next;
            }
            x->substitutions = NULL;
            x->nsubs = 0;
        }
    }

    
    
}

void printargs(int argc, t_atom *argv)
{
    int i;
    for( i = 0; i < argc; i++)
    {
        switch ((argv+i)->a_type) {
            case A_FLOAT:
                post("%s argv[%d] %f", __func__, i, atom_getfloat(argv+i));
                break;
            case A_SYMBOL:
                post("%s argv[%d] %s", __func__, i, atom_getsymbol(argv+i)->s_name);
                break;
            default:
                break;
        }
    }
}

void *omessage_new(t_symbol *msg, short argc, t_atom *argv)
{
    t_omessage *x = (t_omessage *)pd_new(omessage_class->class);
    if(x)
    {
        x->glist = (t_glist *)canvas_getcurrent();
        
        x->outlet = outlet_new(&x->ob, gensym("FullPacket"));
                
//            x->ob.b_firstin = (void *)x;
        
        x->proxy = (void **)malloc(argc * sizeof(t_omax_pd_proxy *));
        x->proxy[0] = proxy_new((t_object *)x, 0, &(x->inlet), omessage_proxy_class);
        x->proxy[1] = proxy_new((t_object *)x, 1, &(x->inlet), omessage_proxy_class);
        
        
        x->bndl = NULL;
        x->substitutions = NULL;
        critical_new(&(x->lock));
//        x->qelem = qelem_new((t_object *)x, (method)omessage_refresh);
        /*
        if(textfield){
            object_attr_setchar(textfield, gensym("editwhenunlocked"), 1);
            textfield_set_editonclick(textfield, 0);
            textfield_set_textmargins(textfield, 3, 3, 3, 3);
            textfield_set_textcolor(textfield, &(x->text_color));
        }
            omessage_gettext(x);
        */
        
        x->m_clock = clock_new(x, (t_method)omessage_tick);
        
        x->text = NULL;
        x->text = (char *)malloc(MAXPDSTRING * sizeof(char));
        
        x->hex = NULL;
        x->hex = (char *)malloc(MAXPDSTRING * sizeof(char));
        
        
        //object name heirarchy:
        char buf[MAXPDSTRING];
        sprintf(buf,".x%lx.c", (long unsigned int)glist_getcanvas(x->glist));
        x->canvas_id = NULL;
        x->canvas_id = (char *)malloc(sizeof(char) * (strlen(buf)+1));
        strcpy(x->canvas_id, buf);
        
        sprintf(buf, ".x%lx.t%lxTEXT", (long unsigned int)glist_getcanvas(x->glist), (long unsigned int)x);
        x->text_id = NULL;
        x->text_id = (char *)malloc(sizeof(char) * (strlen(buf)+1));
        strcpy(x->text_id, buf);
        
        sprintf(buf, ".x%lx.h%lxHANDLE", (long unsigned int)glist_getcanvas(x->glist), (long unsigned int)x);
        x->handle_id = NULL;
        x->handle_id = (char *)malloc(sizeof(char) * (strlen(buf)+1));
        strcpy(x->handle_id, buf);
        
        sprintf(buf, "%lxBORDER", (long unsigned int)x);
        x->border_tag = NULL;
        x->border_tag = (char *)malloc(sizeof(char) * (strlen(buf)+1));
        strcpy(x->border_tag, buf);
        
        sprintf(buf, "%lxIOLETS", (long unsigned int)x);
        x->iolets_tag = NULL;
        x->iolets_tag = (char *)malloc(sizeof(char) * (strlen(buf)+1));
        strcpy(x->iolets_tag, buf);
        
        
        sprintf(buf,"omess%lx",(long unsigned int)x);
        x->tcl_namespace = NULL;
        x->tcl_namespace = (char *)malloc(sizeof(char) * (strlen(buf)+1));
        strcpy(x->tcl_namespace, buf);
        
        sprintf(buf,"#%s", x->tcl_namespace);
        x->receive_name = gensym(buf);
        pd_bind(&x->ob.ob_pd, x->receive_name);
        
        
        printargs(argc, argv);
        
        x->width = 20;
        x->height = 10;
        
        if(argc > 3)
        {
            x->width = atom_getfloat(argv);
            x->height = atom_getfloat(argv+1);
            if((argv+2)->a_type == A_SYMBOL && atom_getsymbol(argv+2) == gensym("hex"))
            {
                omessage_textbuf(x, NULL, 2, (argv+2));
            }
        }
        
        //sys_vgui("namespace eval ::%s [list set textbuf%lx \"%s\"] \n", x->tcl_namespace, glist_getcanvas(x->glist), x->text);
        
        sys_vgui("proc string2hex s {\n\
                 binary scan $s H* hex\n\
                 # regsub -all (..) $hex {\\x\1} \n\
                 return $hex\n\
                 }\n");
        
        sys_vgui("proc string2bytes s {\n\
                 binary scan $s B* hex\n\
                 return $hex\n\
                 }\n");
        
        sys_vgui("proc string2ascii s {\n\
                 binary scan $s c* c_int\n\
                 return $c_int\n\
                 }\n");
        
        // sys_vgui("namespace eval ::%s [list set textbuf%lx \"/foo 1\"] \n", x->tcl_namespace, glist_getcanvas(x->glist));
        
        x->mouseDown = 0;
        x->selected = 0;
        x->editmode = glist_getcanvas(x->glist)->gl_edit;
        x->textediting = 0;
        
        omessage_resetText(x, x->text, 0);
                
        //if possible, draw widget here first and set bindings so we don't have to do it over and over again?
        
    }
    return (void *)x;
}

void o_message_setup(void) {
    
    omax_pd_class_new(omessage_class, gensym("o_message"), (t_newmethod)omessage_new, (t_method)omessage_free, sizeof(t_omessage),  CLASS_NOINLET, A_GIMME, 0);
    
    class_addmethod(omessage_class->class, (t_method)omessage_textbuf,gensym("textbuf"), A_GIMME, 0);
    class_addmethod(omessage_class->class, (t_method)omessage_insideclick_callback, gensym("insideclick"), 0);
    class_addmethod(omessage_class->class, (t_method)omessage_outsideclick_callback, gensym("outsideclick"), 0);
    class_addmethod(omessage_class->class, (t_method)omessage_pdnofocus_callback, gensym("pdnofocus"), 0);
    class_addmethod(omessage_class->class, (t_method)omessage_key_callback, gensym("key"), A_GIMME, 0);
    class_addmethod(omessage_class->class, (t_method)omessage_keyup_callback, gensym("keyup"), A_GIMME, 0);
    
    class_addmethod(omessage_class->class, (t_method)omessage_resize_mousedown, gensym("resize_mousedown"), 0);
    class_addmethod(omessage_class->class, (t_method)omessage_resize_mousemove, gensym("resize_mousemove"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(omessage_class->class, (t_method)omessage_resize_mouseup, gensym("resize_mouseup"), 0);
    
    t_omax_pd_proxy_class *c = NULL;
    omax_pd_class_new(c, NULL, NULL, NULL, sizeof(t_omax_pd_proxy), CLASS_PD | CLASS_NOINLET, 0);
    
    omax_pd_class_addmethod(c, (t_method)odot_version, gensym("version"));
    omax_pd_class_addbang(c, (t_method)omessage_bang);
    
	omax_pd_class_addfloat(c, (t_method)omessage_float);
	omax_pd_class_addmethod(c, (t_method)omessage_list, gensym("list"));
	omax_pd_class_addmethod(c, (t_method)omessage_anything, gensym("anything"));
	omax_pd_class_addmethod(c, (t_method)omessage_set, gensym("set"));
//	class_addmethod(c, (method)omessage_doc, "doc", 0);

	omax_pd_class_addmethod(c, (t_method)omessage_fullPacket, gensym("FullPacket"));
    
    omessage_widgetbehavior.w_getrectfn = omessage_getrect;
    omessage_widgetbehavior.w_displacefn = omessage_displace;
    omessage_widgetbehavior.w_selectfn = omessage_select;
    omessage_widgetbehavior.w_deletefn = omessage_delete;
    omessage_widgetbehavior.w_clickfn = omessage_click;
    omessage_widgetbehavior.w_activatefn = omessage_activate;
    omessage_widgetbehavior.w_visfn = omessage_vis;
    
    
    class_setsavefn(omessage_class->class, omessage_save);
    class_setwidget(omessage_class->class, &omessage_widgetbehavior);
    
    ps_newline = gensym("\n");
	ps_FullPacket = gensym("FullPacket");
    
    omessage_proxy_class = c;

    //return 0;

}

#else
void omessage_free(t_omessage *x)
{
	jbox_free((t_jbox *)x);
	critical_free(x->lock);
	if(x->proxy){
		object_free(x->proxy);
	}
	if(x->bndl){
		switch(x->bndltype){
            case OMESSAGE_S:
                osc_bundle_s_deepFree((t_osc_bndl_s *)x->bndl);
                break;
            case OMESSAGE_U:
                osc_bundle_u_free((t_osc_bndl_u *)x->bndl);
                break;
		}
	}
	if(x->substitutions){
		t_osc_parser_subst *s = x->substitutions;
		while(s){
			t_osc_parser_subst *next = s->next;
			osc_mem_free(s);
			s = next;
		}
		x->substitutions = NULL;
		x->nsubs = 0;
	}
}

OMAX_DICT_DICTIONARY(t_omessage, x, omessage_fullPacket);

void omessage_doc(t_omessage *x)
{
	omax_doc_outletDoc(x->outlet);
}

void omessage_assist(t_omessage *x, void *b, long io, long num, char *buf)
{
	omax_doc_assist(io, num, buf);
}


/**************************************************
 Object and instance creation functions.
 **************************************************/

void *omessage_new(t_symbol *msg, short argc, t_atom *argv){
	t_omessage *x;
    
	t_dictionary *d = NULL;
 	long boxflags;
    
	// box setup
	if(!(d = object_dictionaryarg(argc, argv))){
		return NULL;
	}
    
	boxflags = 0
    | JBOX_DRAWFIRSTIN
    | JBOX_NODRAWBOX
    | JBOX_DRAWINLAST
    | JBOX_TRANSPARENT
    //      | JBOX_NOGROW
    //| JBOX_GROWY
    //| JBOX_GROWBOTH
    //      | JBOX_HILITE
    //| JBOX_BACKGROUND
    //| JBOX_DRAWBACKGROUND
    //      | JBOX_NOFLOATINSPECTOR
    //      | JBOX_MOUSEDRAGDELTA
    | JBOX_TEXTFIELD
    ;
    
	if((x = (t_omessage *)object_alloc(omessage_class))){
		jbox_new((t_jbox *)x, boxflags, argc, argv);
 		x->ob.b_firstin = (void *)x;
		x->outlet = outlet_new(x, NULL);
		x->proxy = proxy_new(x, 1, &(x->inlet));
		x->bndl = NULL;
		x->substitutions = NULL;
		critical_new(&(x->lock));
		x->qelem = qelem_new((t_object *)x, (method)omessage_refresh);
		attr_dictionary_process(x, d);
        
		t_object *textfield = jbox_get_textfield((t_object *)x);
		if(textfield){
			object_attr_setchar(textfield, gensym("editwhenunlocked"), 1);
			textfield_set_editonclick(textfield, 0);
			textfield_set_textmargins(textfield, 3, 3, 3, 3);
			textfield_set_textcolor(textfield, &(x->text_color));
		}
        
 		jbox_ready((t_jbox *)x);
        
		omessage_gettext(x);
		return x;
	}
	return NULL;
}

// CLASS_ATTR_STYLE_LABEL as defined in ext_obex_util.h uses gensym_tr() which isn't included in the SDK causing
// compilation to fail.  This kludge just replaces gensym_tr() with gensym()
/*
 #define CLASS_ATTR_STYLE_LABEL_KLUDGE(c,attrname,flags,stylestr,labelstr) \
 { CLASS_ATTR_ATTR_PARSE(c,attrname,"style",USESYM(symbol),flags,stylestr); CLASS_ATTR_ATTR_FORMAT(c,attrname,"label",USESYM(symbol),flags,"s",gensym(labelstr)); }
 */
#define CLASS_ATTR_CATEGORY_KLUDGE(c,attrname,flags,parsestr) \
CLASS_ATTR_ATTR_PARSE(c,attrname,"category",USESYM(symbol),flags,parsestr)


int main(void){
	common_symbols_init();
	t_class *c = class_new("o.message", (method)omessage_new, (method)omessage_free, sizeof(t_omessage), 0L, A_GIMME, 0);
	alias("o.m");
    
	c->c_flags |= CLASS_FLAG_NEWDICTIONARY;
 	jbox_initclass(c, JBOX_TEXTFIELD | JBOX_FIXWIDTH | JBOX_FONTATTR);
    
	class_addmethod(c, (method)omessage_paint, "paint", A_CANT, 0);
    
	class_addmethod(c, (method)omessage_bang, "bang", 0);
	class_addmethod(c, (method)omessage_int, "int", A_LONG, 0);
	class_addmethod(c, (method)omessage_float, "float", A_FLOAT, 0);
	class_addmethod(c, (method)omessage_list, "list", A_GIMME, 0);
	class_addmethod(c, (method)omessage_anything, "anything", A_GIMME, 0);
	class_addmethod(c, (method)omessage_set, "set", A_GIMME, 0);
	class_addmethod(c, (method)omessage_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)omessage_doc, "doc", 0);
	class_addmethod(c, (method)stdinletinfo, "inletinfo", A_CANT, 0);
	//class_addmethod(c, (method)omessage_fullPacket, "FullPacket", A_LONG, A_LONG, 0);
	class_addmethod(c, (method)omessage_fullPacket, "FullPacket", A_GIMME, 0);
	// remove this if statement when we stop supporting Max 5
	if(omax_dict_resolveDictStubs()){
		class_addmethod(c, (method)omax_dict_dictionary, "dictionary", A_GIMME, 0);
	}
    
	class_addmethod(c, (method)omessage_clear, "clear", 0);
    
	class_addmethod(c, (method)omessage_key, "key", A_CANT, 0);
	class_addmethod(c, (method)omessage_keyfilter, "keyfilter", A_CANT, 0);
	class_addmethod(c, (method)omessage_enter, "enter", A_CANT, 0);
	class_addmethod(c, (method)omessage_select, "select", 0);
    
	class_addmethod(c, (method)omessage_mousedown, "mousedown", A_CANT, 0);
	class_addmethod(c, (method)omessage_mouseup, "mouseup", A_CANT, 0);
    
	class_addmethod(c, (method)odot_version, "version", 0);
    
    
 	CLASS_ATTR_RGBA(c, "background_color", 0, t_omessage, background_color);
 	CLASS_ATTR_DEFAULT_SAVE_PAINT(c, "background_color", 0, ".87 .87 .87 1.");
 	CLASS_ATTR_STYLE_LABEL(c, "background_color", 0, "rgba", "Background Color");
	CLASS_ATTR_CATEGORY_KLUDGE(c, "background_color", 0, "Color");
    
 	CLASS_ATTR_RGBA(c, "frame_color", 0, t_omessage, frame_color);
 	CLASS_ATTR_DEFAULT_SAVE_PAINT(c, "frame_color", 0, "0. 0. 0. 1.");
 	CLASS_ATTR_STYLE_LABEL(c, "frame_color", 0, "rgba", "Frame Color");
	CLASS_ATTR_CATEGORY_KLUDGE(c, "frame_color", 0, "Color");
    
 	CLASS_ATTR_RGBA(c, "text_color", 0, t_omessage, text_color);
 	CLASS_ATTR_DEFAULT_SAVE_PAINT(c, "text_color", 0, "0. 0. 0. 1.");
 	CLASS_ATTR_STYLE_LABEL(c, "text_color", 0, "rgba", "Text Color");
	CLASS_ATTR_CATEGORY_KLUDGE(c, "text_color", 0, "Color");
    
	CLASS_ATTR_DEFAULT(c, "rect", 0, "0. 0. 150., 18.");
    
	class_register(CLASS_BOX, c);
	omessage_class = c;
    
	ps_newline = gensym("\n");
	ps_FullPacket = gensym("FullPacket");
    
	ODOT_PRINT_VERSION;
    
	return 0;
}

#endif