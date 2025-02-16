#include "ext.h"
#include "ext_obex.h"

#include "xlsxwriter.h"


typedef struct _xlsxw {
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
} t_xlsxw;




// method prototypes
void* xlsxw_new(t_symbol* s, long argc, t_atom* argv);
void xlsxw_free(t_xlsxw* x);
void xlsxw_bang(t_xlsxw* x);
void xlsxw_float(t_xlsxw *x, double f);
t_max_err xlsxw_name_get(t_xlsxw *x, t_object *attr, long *argc, t_atom **argv);
t_max_err xlsxw_name_set(t_xlsxw *x, t_object *attr, long argc, t_atom *argv);
void xlsxw_dict(t_xlsxw* x, t_symbol* s);

// global class pointer variable
static t_class* xlsxw_class = NULL;


void ext_main(void* r)
{
    t_class* c = class_new("xlsxw", (method)xlsxw_new,
                           (method)xlsxw_free, (long)sizeof(t_xlsxw), 0L,
                           A_GIMME, 0);

    class_addmethod(c, (method)xlsxw_bang,  "bang", 0);
    class_addmethod(c, (method)xlsxw_float, "float", A_FLOAT, 0);

    CLASS_ATTR_LABEL(c,     "name", 0,  "patch-wide name");
    CLASS_ATTR_SYM(c,       "name", 0,  t_xlsxw, name);
    CLASS_ATTR_BASIC(c,     "name", 0);
    CLASS_ATTR_SAVE(c,      "name", 0);
    CLASS_ATTR_ACCESSORS(c, "name", xlsxw_name_get, xlsxw_name_set);

    class_register(CLASS_BOX, c);
    xlsxw_class = c;
}


void* xlsxw_new(t_symbol* s, long argc, t_atom* argv)
{
    t_xlsxw* x = (t_xlsxw*)object_alloc(xlsxw_class);

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

void xlsxw_free(t_xlsxw* x) {
    object_free(x->ob_proxy_2);
    object_free(x->ob_proxy_1);
}


void xlsxw_float(t_xlsxw *x, double f)
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
        error("xlsxw_float switch out-of-index");
    }
}


void xlsxw_demo(t_xlsxw* x)
{
    /* Create a new workbook and add a worksheet. */
    lxw_workbook  *workbook  = workbook_new("xlsxw_demo.xlsx");
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);

    /* Add a format. */
    lxw_format *format = workbook_add_format(workbook);

    /* Set the bold property for the format */
    format_set_bold(format);

    /* Change the column width for clarity. */
    worksheet_set_column(worksheet, 0, 0, 20, NULL);

    /* Write some simple text. */
    worksheet_write_string(worksheet, 0, 0, "Hello", NULL);

    /* Text with formatting. */
    worksheet_write_string(worksheet, 1, 0, "World", format);

    /* Write some numbers. */
    worksheet_write_number(worksheet, 2, 0, 123,     NULL);
    worksheet_write_number(worksheet, 3, 0, 123.456, NULL);

    /* Insert an image. */
    // worksheet_insert_image(worksheet, 1, 2, "logo.png");

    workbook_close(workbook);
}


void xlsxw_bang(t_xlsxw* x) {

    xlsxw_demo(x);

    outlet_bang(x->outlet); 
}


t_max_err xlsxw_name_get(t_xlsxw *x, t_object *attr, long *argc, t_atom **argv)
{
    char alloc;

    if (argc && argv) {
        if (atom_alloc(argc, argv, &alloc)) {
                return MAX_ERR_OUT_OF_MEM;
            }
            if (alloc) {
                atom_setsym(*argv, x->name);
                post("xlsxw_name_get: %s", x->name->s_name);
        }
    }
    return MAX_ERR_NONE;
}


t_max_err xlsxw_name_set(t_xlsxw *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc && argv) {
        x->name = atom_getsym(argv);
        post("xlsxw_name_set: %s", x->name->s_name);
    }
    return MAX_ERR_NONE;
}



