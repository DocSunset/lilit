#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

char ATSIGN = 64; /* ascii for at sign */
const char CTRL = 7; /* ascii ^G, aka BEL */
int line_number = 0;

typedef struct List list;

typedef struct CodeChunk
{
    char * name;
    char * contents;
    int tangle;
} code_chunk;


code_chunk * code_chunk_new(char * name)
{
    code_chunk * chunk = malloc(sizeof(code_chunk));
    chunk->name     = name;
    chunk->contents = NULL;
    chunk->tangle   = 0;
    return chunk;
}

struct List
{
    code_chunk * chunk;
    struct List * successor;
};

list * list_new(code_chunk * c)
{
    list * l = malloc(sizeof(list));
    l->chunk = c;
    l->successor = NULL;
}

void list_append(list * l, list * addend)
{
    if (l == NULL)
    {
        fprintf(stderr, "Error: attempt to list append to NULL on line %d\n", line_number);
        exit(1);
    }
    while (l->successor != NULL) l = l->successor; /* go to the end of l */
    l->successor = addend;
}

/* http://www.cse.yorku.ca/* http://www.cse.yorku.ca/ http://www.cse.yorku.ca/http://www.cse.yorku.ca/ttp://www.cse.yorku.ca/tp://www.cse.yorku.ca/p://www.cse.yorku.ca/://www.cse.yorku.ca///www.cse.yorku.ca//www.cse.yorku.ca/www.cse.yorku.ca/ww.cse.yorku.ca/w.cse.yorku.ca/.cse.yorku.ca/cse.yorku.ca/se.yorku.ca/e.yorku.ca/.yorku.ca/yorku.ca/orku.ca/rku.ca/ku.ca/u.ca/.ca/ca/a//oz/hash.html */
unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

typedef struct Dict
{
    list ** array;
    size_t size;
} dict;

dict * dict_new(size_t size)
{
    dict * d = malloc(sizeof(dict));
    d->array = malloc(size * sizeof(list));
    memset(d->array, (size_t)NULL, size * sizeof(list));
    d->size = size;
}

void dict_add(dict * d, code_chunk * c)
{
    unsigned long h = hash(c->name) % d->size;
    list * l = list_new(c);
    if (d->array[h] == NULL) d->array[h] = l;
    else list_append(d->array[h], l);
}

code_chunk * dict_get(dict * d, char * name)
{
    unsigned long h = hash(name) % d->size;
    list * l = d->array[h];
    while (l != NULL)
    {
        if (strcmp(name, l->chunk->name) == 0) return l->chunk;
        else l = l->successor;
    }
    return NULL;
}


char * duplicate_line_and_increment(char ** s)
{
    char * selection_start = *s;
    char * destination;
    char * selection_end;

    while (isspace(*selection_start)) ++selection_start; 
    selection_end = strchr(selection_start, '\n');
    while (isspace(*selection_end)) --selection_end; 
    ++selection_end; /* point one past the last non-whitespace character */

    if (selection_end - selection_start == 0) return NULL;

    *selection_end = '\0';
    destination = malloc(strlen(selection_start) + 1);
    strcpy(destination, selection_start);
    *selection_end = '\n';

    *s = selection_end + 1; /* s points after newline */
    line_number += 1;

    return destination;
}

char * extract_invocation_name(char * s)
{
    char * selection_start = s;
    char * destination;
    char * selection_end = strchr(selection_start, '}');
    if (selection_end == NULL)
    {
        fprintf(stderr,
                "Error: unterminated chunk invocation on line %d\n",
                line_number);
        exit(1);
    }
    if (selection_end - selection_start == 0) 
    {
        fprintf(stderr,
                "Error: empty chunk invocation on line %d\n",
                line_number);
        exit(1);
    }
    *selection_end = '\0';
    destination = malloc(strlen(selection_start) + 1);
    strcpy(destination, selection_start);
    *selection_end = '}';
    return destination;
}

void code_chunk_print(FILE * f, dict * d, code_chunk * c, char * indent, int indented)
{
    char * s = c->contents;
    /* https://stackoverflow.com/questions/17983005/c-how-to-read-a-string-line-by-line */
    while(s && *s)
    {
        if (!indented) fprintf(f, "%s", indent);
        char * invocation;
        char * next_newline_char = strchr(s, '\n');
        if (next_newline_char) *next_newline_char = '\0';  /* terminate the current line */

        invocation = strchr(s, CTRL);
        while (invocation != NULL)
        {
            code_chunk * next_c;
            char tmp;
            char * name;
            char * indent_end;
            char * next_indent;

            /* print whatever is before the invocation */
            *invocation = '\0';
            fprintf(f, "%s", s);
            *invocation = CTRL;

            /* check if the CTRL character is from an escape sequence */
            if (!(*(invocation + 1) == '{' || *(invocation + 1) == '*'))
            {
                ++s;
                invocation = strchr(s, CTRL);
                continue;
            }

            /* build the next indent */
            indent_end = s; while (isspace(*indent_end)) ++indent_end;
            tmp = *indent_end;
            *indent_end = '\0';
            next_indent = malloc(strlen(s) + strlen(indent) + 1);
            strcpy(next_indent, s); 
            strcat(next_indent, indent);
            *indent_end = tmp;

            /* print the invocation itself */
            invocation = strchr(invocation, '{') + 1; 
            name = extract_invocation_name(invocation);
            next_c = dict_get(d, name);
            if (next_c == NULL)
            {
                fprintf(stderr,
                        "Warning: invocation of chunk '%s' could not be found\n",
                        name);
            }
            else code_chunk_print(f, d, next_c, next_indent, 1);

            s = strchr(s, '}') + 1;
            invocation = strchr(s, CTRL);
        }
        /* print a whole line with no invocation, or the remainder of the line
         * following any number of invocations */
        fprintf(f, "%s", s); 

        if (next_newline_char) 
        {
            *next_newline_char = '\n';  /* restore newline-char */
            fprintf(f, "\n");
            indented = 0;
        }
        s = next_newline_char ? (next_newline_char+1) : NULL;
    }
}


const char * help =
"Control sequences: \n\
\n\
Control sequences are permitted to appear at any point in the source file, \n\
except for flags which must appear inside of a code chunk. All control sequences\n\
begin with a special character called ATSIGN, which is set to '@' by default.\n\
\n\
\n\
ATSIGN:new control character  Redefines ATSIGN to whatever immediately follows\n\
                              the : sign. This is useful if your machine source\n\
                              has lots of @ signs, for example.\n\
\n\
ATSIGN='chunk name'           Begin a regular chunk declaration.\n\
\n\
ATSIGN#'chunk name'           Begin a tangle chunk declaration. This is similar\n\
                              to a regular chunk, except the name of the chunk\n\
                              is also interpreted as a file name, and the chunk\n\
                              is recursively expanded into the file with that\n\
                              name, overwriting any existing file.\n\
\n\
ATSIGN                        End a chunk declaration. The ATSIGN must be \n\
                              immediately followed by a newline character or the\n\
                              end of the file without intervening white space.\n\
\n\
ATSIGN{chunk invocation}      Invoke a chunk to be recursively expanded into any\n\
                              tangled output files.\n\
\n\
ATSIGNATSIGN                  Escape sequence. A literal ATSIGN sign with no\n\
                              special meaning to lilit that will be copied as an\n\
                              ATSIGN to any output tangled or woven documents.\n\
\n\
";



int main(int argc, char ** argv)
{
    int file_size;
    char * source;
    dict * d;
    list * tangles = NULL;

    if (argc < 2 || *argv[1] == '-' /* assume -h */) 
    {
        printf("%s", help);
        exit(0);
    }
    char * filename = argv[1];

    {
        FILE * source_file = fopen(filename, "r");
        if (source_file == NULL)
        {
            fprintf(stderr, "Error: could not open file %s\n", filename);
            exit(1);
        }
        
        /* get file size */
        fseek(source_file, 0L, SEEK_END);
        file_size = ftell(source_file);
        rewind(source_file);
        
        /* copy file into memory */
        source = malloc(1 + file_size); /* one added for null termination */
        fread(source, 1, file_size, source_file); 
        fclose(source_file);
        
        source[file_size] = 0;
    }

    d = dict_new(128); /* for storing chunks */


    {
        char * s = source;
        
        while (*s != '\0') 
        {
            if (*s == '\n')
            {
                ++line_number;
            }
            else if (*s == ATSIGN)
            {
                /* ATSIGNs are converted to CTRL so that we can find them again
                 * later even in case ATSIGN is changed to some other character
                 * in the interim */
                *s++ = CTRL; /* set ATSIGN to CTRL and increment s */
    
                if (*s == '=' || *s == '#')
                {
                    int tangle = *s++ == '#';
                    code_chunk * c = code_chunk_new(duplicate_line_and_increment(&s));
                    c->contents = s;
                    c->tangle = tangle;
                    
                    if (c->name == NULL)
                    {
                        fprintf(stderr,
                                "Error: expected code chunk name on line %d.\n",
                                line_number);
                        exit(1);
                    }
                    
                    for (; *s; ++s)
                    {
                        if (*s == ATSIGN) *s = CTRL;
                        else if (*s == '\n')
                        {
                            ++line_number;
                            if (*(s - 1) == CTRL)
                            {
                                *(s - 1) = '\0'; 
                                dict_add(d, c);
                                if (tangles == NULL) tangles = list_new(c);
                                else list_append(tangles, list_new(c));
                                break;
                            }
                        }
                    }

                }
                else if (*s++ == ':')
                {
                    if  (  *s == ':' 
                        || *s == '=' 
                        || *s == '#' 
                        || *s == '\n' 
                        || *s == '{' 
                        )
                    {
                        fprintf(stderr,
                                "Error: cannot redefine ATSIGN to a character used in control sequences on line %d\n",
                                line_number);
                        exit(1);
                    }
                    else ATSIGN = *s;

                }
            }
            ++s;
        }
    }

    for(; tangles != NULL; tangles = tangles->successor)
    {
        FILE * f;
        code_chunk * c = tangles->chunk;
        f = fopen(c->name, "w");
        if (f == NULL)
        {
            fprintf(stderr,
                    "Warning: failed to open file '%s', skipping tangle '%s'\n",
                    c->name, c->name);
            continue;
        }
        code_chunk_print(f, d, c, "", 0);
        fclose(f);
    }

}
