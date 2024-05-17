/*
 * FONCTIONNEMENT GENERAL :
 * Les fichiers sont lu un à un avec fgetc, chaque lettre est d'abord stocké
 * dans le tableau de caractère "mot" de taille 100 (temporaire ?), quand le
 * caractère lu est de type isspace(), on cherche dans la table de hashage le
 * mot
 * en question.
 * Si la recherche est négative on alloue une chaine de caractère s de la taille
 * du mot qui a été lu, et on copie mot dans s.
 * On a donc notre mot sous forme de pointeur, prêt à être envoyé dans le
 * fourretout / table de hashage.
 * On ajoute à la table de hashage, la clé qui sera la chaine de caractère, et
 * sa
 * valeur qui sera le couple nombre d'occurrence du mot, numéro du fichier.
 * Ce couple permet de savoir très facilement à quel fichier appartient quel mot
 * et son nombre d'occurrence dans ce fichier.
 * La lecture des mots continue de se faire à chaque tour de boucle, la chaine
 * mot est reinitialisé grâce à l'appel à la fonction memset
 *
 * Si la recherche est positive, on test si le mot provient du même fichier
 * qu'on
 * est entrain de lire.
 * Si c'est le cas on augmente juste l'occurrence de 1, sinon ça veux dire que
 * le
 * mot n'est pas exclusif au fichier, et on met l'occurrence à -1, ce qui le met
 * de côté, le mot sera toujours présent dans la table de hashage mais il sera
 * marqué comme "non exclusif".
 *
 * Quand le caractère lu par fgetc est EOF, on ferme le fichier, si il y en a
 * d'autres ils sont ouvert puis lu de la même façons.
 * Une fois que tous les fichiers ont été lu, on applique au fourretout qui
 * stocke les chaines alloués, la fonction display, qui affiche à l'écran les
 * chaines de caractères.
 * Cette fonction affiche le mot que si son occurrence n'est pas -1, autrement
 * dit que si il est exclusif.
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <ctype.h>
#include "hashtable.h"
#include "holdall.h"

#define BASE_WORD_SIZE 100

#define NON_EXCLUSIVE_FILE -1

#define OPT_SHORT "-"
#define OPT_LONG "--"
#define OPT_HELP OPT_SHORT "?"
#define OPT_I OPT_SHORT "i"
#define OPT_P OPT_SHORT "p"
#define OPT_USAGE OPT_LONG "usage"
#define OPT_VERSION OPT_LONG "version"
#define OPT_IS_ACTIVE(o) options.o == true

#define DIRECT_INPUT 1
#define FILE_INPUT 0

//occur_file : Structure visant à sauvegarder sous forme d'un couple, les
//  informations relatives aux mots lus dans un fichier : occur correspond au
//  nombre d'occurrences d'un mot dans un fichier, nfile correspond au numéro
//  du fichier dans lequel le mot a été lu.
typedef struct occur_file occur_file;
struct occur_file {
  int occur;
  int nfile;
};

//opt : Structure gérante des options entrées lors de l'execution du programme
//  .help correspond à l'option -?, elle vaut true si elle a été entrée
//  .i correspond à l'option -i VALUE, sa valeur correspond à VALUE
//  .p et r correspondent aux options de mêmes noms, elles valent true si elles
//  ont été entrée
//  .direct correspond au nombre de "fichier" lu sur l'entrée direct au clavier
typedef struct opt opt;
struct opt {
  long int i;
  bool p;
  int direct;
};

//  str_hashfun : l'une des fonctions de pré-hachage conseillées par Kernighan
//    et Pike pour les chaines de caractères.
size_t str_hashfun(const char *s);

//  display : affiche à l'écran la chaine de caractère pointé par str, ainsi que
//    son nombre d'occurrence, pointé par le champs occur de info.
int display(const char *str, occur_file *info);

//  rfree : libère l'adresse pointé par p.
int rfree(void *p);

//  get_options : Lit le tableau av et active les options qui ont été entrée
//    par l'utilisateur lors de l'appel du programme xwc. Renvoie le nombre
//    d'options qui a été entré. Av correspond au tableau argv de la fonction
//    main, ac à argc de la fonction main, options est la structure qui stocke
//    les informations des différents options entrées ou non.
int get_options(char **av, int ac, opt *options);

//  get_input_type : Donne le type d'entrée pour l'execution du programme,
//    renvoie 1 si l'entrée se fait au clavier directement, 0 si l'entrée se
//    fait par lecture de fichier.
int get_input_type(opt *options);

//  select_input : Selectionne l'entrée qui a été detecter pour l'execution du
//    programme, type correspond au retour de la fonction get_input_type
//    file est le nom du fichier à lire dans le cas ou l'entrée se fait par
//    lecture de fichier. Renvoie stdin en cas d'entrée direct, sinon un FILE *f
//    qui correspond à l'ouverture d'un fichier, renvoie NULL si cette ouverture
//    a échoué.
FILE *select_input(int type, char *file);

//  is_end_of_word : Teste si le caractère C correspond à la fin d'un mot.
bool is_end_of_word(int c, opt *options, size_t i);

//  print_help : Affiche sur l'entrée standart l'aide du programme xwc.
void print_help();

//  print_usage : Affiche un message court sur l'utilisation du programme xwc.
void print_usage();

//  print_version : Affiche la version du programme xwc.
void print_version();

int main(int argc, char **argv) {
  int r = EXIT_SUCCESS;
  opt options;
  int nb_options = get_options(argv, argc, &options);
  if (nb_options == -1) {
    fprintf(stderr, "*** Error : Incorrect option\n");
    return EXIT_FAILURE;
  }
  //setlocale : pour la fonction strcoll
  setlocale(LC_COLLATE, "");
  //allocation de la table de hashage
  hashtable *ht = hashtable_empty((int (*)(const void *, const void *))strcoll,
      (size_t (*)(const void *))str_hashfun);
  //allocation des fourretouts
  holdall *ha = holdall_empty();
  holdall *ha2 = holdall_empty();
  if (ht == NULL
      || ha == NULL
      || ha2 == NULL) {
    goto error_capacity;
  }
  //mot : buffer qui stocke les lettres lu du fichier
  size_t bufsiz = BASE_WORD_SIZE;
  char *mot = malloc(bufsiz + 1);
  if (mot == NULL) {
    goto error_capacity;
  }
  //nfile : numéro du fichier lu (nfile = arg - nb options) (simplifie le code)
  int nfile = 1;
  //arg : numéro de l'argument traité
  int arg = 1 + nb_options - options.direct;
  int input_type = get_input_type(&options);
  while (arg < argc) {
    FILE *f = select_input(input_type, argv[arg]);
    if (f == NULL) {
      goto error;
    }
    if (input_type == DIRECT_INPUT) {
      printf("--- starts reading for #%d FILE\n", nfile);
    }
    size_t i = 0;
    int nline = 1;
    int c = 0;
    while (c != EOF) {
      c = fgetc(f);
      if (is_end_of_word(c, &options, i)) {
        //le mot est fini, on met le \0
        mot[i] = '\0';
        //si l'options i est activé
        if (options.i > 0) {
          //detecte si le mot a été coupé
          if (!isspace(c)) {
            fprintf(stderr, "%s: Word from file '%s' at line %d cut: '%s...'\n",
                argv[0], argv[arg], nline, mot);
          }
          //passe jusqu'au prochain espace
          while (!isspace(c) && c != EOF) {
            c = fgetc(f);
          }
        }
        //skip jusqu'au prochain mot
        while ((isspace(c) || (OPT_IS_ACTIVE(p) && ispunct(c))) && c != EOF) {
          if (c == '\n') {
            nline++;
          }
          c = fgetc(f);
        }
        occur_file *info = hashtable_search(ht, mot);
        //si on a pas trouvé dans la table,on alloue info et on l'ajoute
        if (info == NULL) {
          //allocation du couple info, nfile
          info = malloc(sizeof *info);
          if (info == NULL) {
            goto error_capacity;
          }
          info->occur = 1;
          info->nfile = nfile;
          //allocation de la string
          char *t = malloc(i + 1);
          if (t == NULL) {
            goto error_capacity;
          }
          //copie dans t
          strcpy(t, mot);
          //ajout aux fourretouts
          if (holdall_put(ha, t) != 0) {
            goto error_capacity;
          }
          if (holdall_put(ha2, info) != 0) {
            goto error_capacity;
          }
          //ajout à la table
          if (hashtable_add(ht, t, info) == NULL) {
            goto error_capacity;
          }
        } else {
          //si on a trouvé le mot mais qu'il ne provient pas du même fichier
          //le mot n'est pas exclusif : occur = -1
          if (info->nfile != nfile) {
            info->occur = NON_EXCLUSIVE_FILE;
          }
          //on a trouvé le mot mais il provient du même fichier : occur++
          else {
            info->occur += 1;
          }
          if (hashtable_add(ht, mot, info) == NULL) {
            goto error_capacity;
          }
        }
        //reinitialisation du tableau de caractère mot
        memset(mot, 0, i);
        i = 0;
      }
      mot[i] = (char) c;
      i++;
      //si on a un mot trop long, on réalloue x2 la taille du buffer
      if (i == bufsiz) {
        bufsiz = bufsiz * 2;
        mot = realloc(mot, bufsiz + 1);
        if (mot == NULL) {
          goto error_capacity;
        }
      }
    }
    if (input_type == FILE_INPUT) {
      if (!feof(f)) {
        goto error_read;
      }
      if (fclose(f) != 0) {
        fprintf(stderr, "*** Failed to close the file : %s\n", argv[arg]);
        goto error;
      }
    } else {
      clearerr(f);
      printf("--- ends reading for #%dFILE\n", nfile);
    }
    nfile++;
    arg++;
  }
  //print de la première ligne de l'affichage
  printf("\t");
  for (int k = 1 + nb_options - options.direct; k < argc; k++) {
    (options.direct > 0) ? printf("\"\"") : printf("%s", argv[k]);
    (k == argc - 1) ? printf("\n") : printf("\t");
  }
  if (holdall_apply_context(ha, ht,
      (void *(*)(void *, void *))hashtable_search,
      (int (*)(void *, void *))display) != 0) {
    goto error_write;
  }
  free(mot);
  goto dispose;
error_read:
  fprintf(stderr, "*** Error: A read error occurs\n");
  goto error;
error_write:
  fprintf(stderr, "*** Error: A write error occurs\n");
  goto error;
error_capacity:
  fprintf(stderr, "*** Error: Not enough memory\n");
  goto error;
error:
  r = EXIT_FAILURE;
  goto dispose;
dispose:
  hashtable_dispose(&ht);
  if (ha != NULL) {
    holdall_apply(ha, rfree);
  }
  if (ha2 != NULL) {
    holdall_apply(ha2, rfree);
  }
  holdall_dispose(&ha);
  holdall_dispose(&ha2);
  return r;
}

size_t str_hashfun(const char *s) {
  size_t h = 0;
  for (const unsigned char *p = (const unsigned char *) s; *p != '\0'; ++p) {
    h = 37 * h + *p;
  }
  return h;
}

int display(const char *str, occur_file *info) {
  if (info->occur != NON_EXCLUSIVE_FILE) {
    printf("%s\t", str);
    for (int i = 1; i < info->nfile; i++) {
      printf("\t");
    }
    printf("%d\n", info->occur);
  }
  return 0;
}

int rfree(void *p) {
  free(p);
  return 0;
}

int get_input_type(opt *options) {
  return (options->direct > 0) ? DIRECT_INPUT : FILE_INPUT;
}

FILE *select_input(int type, char *file) {
  if (type == DIRECT_INPUT) {
    return stdin;
  } else {
    FILE *f = fopen(file, "r");
    if (f == NULL) {
      fprintf(stderr, "*** Failed to open the file : %s\n", file);
      return NULL;
    }
    return f;
  }
}

int get_options(char **av, int ac, opt *options) {
  int r = 0;
  options->p = false;
  options->i = 0;
  options->direct = 0;
  //si pas de fichier entré, on utilise l'entrée direct
  if (ac == 1) {
    options->direct = 1;
  }
  for (int i = 1; i < ac; i++) {
    if (strncmp(av[i], OPT_SHORT, strlen(OPT_SHORT)) == 0
        || strncmp(av[i], OPT_LONG, strlen(OPT_LONG)) == 0) {
      //-?
      if (strcmp(av[i], OPT_HELP) == 0) {
        print_help();
      }
      //usage
      else if (strcmp(av[i], OPT_USAGE) == 0) {
        print_usage();
      }
      //version
      else if (strcmp(av[i], OPT_VERSION) == 0) {
        print_version();
      }
      //-p
      else if (strcmp(av[i], OPT_P) == 0) {
        options->p = true;
      }
      //-iVALUE
      else if (strncmp(av[i], OPT_I, strlen(OPT_I)) == 0) {
        if (!isdigit(av[i][strlen(OPT_I)])) {
          return -1;
        }
        options->i = strtol(av[i] + strlen(OPT_I), NULL, 10);
      }
      //input direct
      else if (strcmp(av[i], OPT_SHORT) == 0) {
        options->direct += 1;
      } else {
        return -1;
      }
      r++;
    } else {
      return r;
    }
  }
  return r;
}

bool is_end_of_word(int c, opt *options, size_t i) {
  return isspace(c) || c == EOF
    || (options->i > 0 && (long int) i == options->i)
    || (options->p == true && ispunct(c));
}

void print_help() {
  printf("Usage: xwc [OPTION]... [FILE]...\n\n"
      "Exclusive word counting. Print the number of occurrences of each word "
      "that\n"
      "appears in one and only one of given text FILES. Process the words using"
      " a\n"
      "hash table\n\n"
      "A word is, by default, a maximum length sequence of characters that do"
      " not\n"
      "belong to the white-space characters set.\n\n"
      "Results are displayed in columns on the standard output. Columns are "
      "separated\n"
      "by the tab character. Lines are terminated by the end-of-line character."
      " A\n"
      "header line shows the FILE names: the name of the first FILE appears in "
      "the\n"
      "second column, that of the second in the third, and so on. For the "
      "following\n"
      "lines, a word appears in the first column, its number of occurrences "
      "in the FILE\n"
      "in which it appears to the exclusion of all others in the column "
      "associated with\n"
      "the FILE. No tab characters are written on a line after the number of\n"
      "occurrences.\n\n"
      "Read the standard input when no FILE is given or for any FILE which is "
      "\"-\". In\n"
      "such cases, " " is displayed in the column associated with the FILE on "
      "the header\n"
      "line.\n\n"
      "The locale specified by the environment affects sort order. Set "
      "'LC_ALL=C' to\n"
      "get the traditional sort order that uses native byte values.\n\n"
      "Mandatory arguments to long options are mandatory for short options too."
      "\n\n"
      "Program Information\n"
      "-?,    Print this help message and exit.\n\n"
      "--usage   Print a short usage message and exit.\n\n"
      "--version    Print version information.\n\n"
      "Input Control\n"
      "-i,    Set the maximal number of significant initial letters\n"
      "for words to VALUE. 0 means without limitation. Default is 0.\n\n"
      "-p,    Make the punctuation characters play the same\n"
      "role as white-space characters in the meaning of words.\n\n");
  exit(EXIT_SUCCESS);
}

void print_usage(void) {
  printf("Usage: xwc [OPTION]... [FILE]...\n");
  exit(EXIT_SUCCESS);
}

void print_version(void) {
  printf("xwc - 24.05.11\n");
  exit(EXIT_SUCCESS);
}
