#include "ext.h"
#include "ext_obex.h"

#include <matplot/matplot.h>



typedef struct _matplotx {
    t_object ob;

    void* outlet;         // outlet

    void *ob_proxy_1;     // inlet proxy
    void *ob_proxy_2;     // inlet proxy
    long ob_inletnum;     // # of inlet currently in use

    t_symbol* name;

    // params
    float param0;
    float param1;
    float param2;
} t_matplotx;




// method prototypes
void* matplotx_new(t_symbol* s, long argc, t_atom* argv);
void matplotx_free(t_matplotx* x);
void matplotx_bang(t_matplotx* x);
void matplotx_float(t_matplotx *x, double f);
t_max_err matplotx_name_get(t_matplotx *x, t_object *attr, long *argc, t_atom **argv);
t_max_err matplotx_name_set(t_matplotx *x, t_object *attr, long argc, t_atom *argv);
void matplotx_dict(t_matplotx* x, t_symbol* s);

// global class pointer variable
static t_class* matplotx_class = NULL;


void ext_main(void* r)
{
    t_class* c = class_new("matplotx", (method)matplotx_new,
                           (method)matplotx_free, (long)sizeof(t_matplotx), 0L,
                           A_GIMME, 0);

    class_addmethod(c, (method)matplotx_bang,  "bang", 0);
    class_addmethod(c, (method)matplotx_float, "float", A_FLOAT, 0);

    CLASS_ATTR_LABEL(c,     "name", 0,  "patch-wide name");
    CLASS_ATTR_SYM(c,       "name", 0,  t_matplotx, name);
    CLASS_ATTR_BASIC(c,     "name", 0);
    CLASS_ATTR_SAVE(c,      "name", 0);
    CLASS_ATTR_ACCESSORS(c, "name", matplotx_name_get, matplotx_name_set);

    class_register(CLASS_BOX, c);
    matplotx_class = c;
}


void* matplotx_new(t_symbol* s, long argc, t_atom* argv)
{
    t_matplotx* x = (t_matplotx*)object_alloc(matplotx_class);

    if (x) {
        x->outlet = outlet_new(x, NULL); // outlet

        // inlet proxy
        x->ob_inletnum = 0;
        // x->ob_proxy_2 = proxy_new(x, 2, &x->ob_inletnum);
        // x->ob_proxy_2 = proxy_new(x, 2, &x->ob_inletnum);
        // x->ob_proxy_1 = proxy_new(x, 1, &x->ob_inletnum);
        x->ob_proxy_2 = proxy_new(x, 2, NULL);
        x->ob_proxy_1 = proxy_new(x, 1, NULL);

        x->name = gensym("");

        x->param0 = 7.0;
        x->param1 = 500.0;
        x->param2 = 250.0;

        attr_args_process(x, argc, argv);

        post("x->param0: %f", x->param0);
        post("x->param1: %f", x->param1);
        post("x->param2: %f", x->param2);

        post("x->name: %s", x->name->s_name);
    }
    return (x);
}

void matplotx_free(t_matplotx* x) {
    object_free(x->ob_proxy_2);
    object_free(x->ob_proxy_1);
}


void matplotx_float(t_matplotx *x, double f)
{
    switch (proxy_getinlet((t_object *)x))
    {
    case 0:
        post("param0 f: %f", f);
        break;

    case 1:
        post("param1 f: %f", f);
        break;

    case 2:
        post("param2 f: %f", f);
        break;

    default:
        error("matplotx_float switch out-of-index");
    }
}


void matplotx_plot(t_matplotx* x) {

    std::vector<double> xs = {29, 17, 14, 13, 12, 4, 11};
    matplot::bar(xs);
    matplot::save("matplotx_bar.svg", "svg");
    // matplot::show();
    post("saved matplotx_bar.svg");
}

void matplotx_bang(t_matplotx* x) {
    matplotx_plot(x); // <- doesn't worked
    outlet_bang(x->outlet); 
}


t_max_err matplotx_name_get(t_matplotx *x, t_object *attr, long *argc, t_atom **argv)
{
    char alloc;

    if (argc && argv) {
        if (atom_alloc(argc, argv, &alloc)) {
                return MAX_ERR_OUT_OF_MEM;
            }
            if (alloc) {
                atom_setsym(*argv, x->name);
                post("matplotx_name_get: %s", x->name->s_name);
        }
    }
    return MAX_ERR_NONE;
}


t_max_err matplotx_name_set(t_matplotx *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc && argv) {
        x->name = atom_getsym(argv);
        post("matplotx_name_set: %s", x->name->s_name);
    }
    return MAX_ERR_NONE;
}



