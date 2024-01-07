
#include <configuration.h>
#include <stddef.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>




typedef enum {DATE_SIZE_ONLY, NO_PARALLEL, DRY_RUN} long_opt_values;


/*!
 * @brief function display_help displays a brief manual for the program usage
 * @param my_name is the name of the binary file
 * This function is provided with its code, you don't have to implement nor modify it.
 */
void display_help(char *my_name) {
    printf("%s [options] source_dir destination_dir\n", my_name);
    printf("Options: \t-n <processes count>\tnumber of processes for file calculations\n");
    printf("         \t-h display help (this text)\n");
    printf("         \t--date_size_only disables MD5 calculation for files\n");
    printf("         \t--no-parallel disables parallel computing (cancels values of option -n)\n");
    printf("         \t--dry-run lists the changes that would need to be synchronized but doesn't perform them\n");
    printf("         \t-v enables verbose mode\n");
}


/*!
 * @brief init_configuration initializes the configuration with default values
 * @param the_config is a pointer to the configuration to be initialized
 */
void init_configuration(configuration_t *the_config) { // Initialise les paramètres par défaut
    the_config->is_parallel = true;
    the_config->is_dry_run = false;
    the_config->is_verbose = false;
    the_config->uses_md5 = true;
    the_config->processes_count = 1;
}


/*!
 * @brief set_configuration updates a configuration based on options and parameters passed to the program CLI
 * @param the_config is a pointer to the configuration to update
 * @param argc is the number of arguments to be processed
 * @param argv is an array of strings with the program parameters
 * @return -1 if configuration cannot succeed, 0 when ok
 */
int set_configuration(configuration_t *the_config, int argc, char *argv[]) {
    if (argc > 8 || argc < 3) { // Teste si le bon nombre d'arguments sont passés en paramètres
        display_help("lp25-backup"); // Affichage de l'aide
        return -1; // Nombre d'argument incorrecte
    } else {
        strcpy(the_config->source, argv[argc-2]); // -2 car avant-dernier élément
        strcpy(the_config->destination, argv[argc-1]); // -1 car dernier élément
        if (argc != 3) { // D'autres arguments donnés que la source et la destination
            int opt;
            struct option long_options[] = {
                    {"date-size-only", no_argument, NULL, DATE_SIZE_ONLY}, // Option longue pour ne pas utiliser la sommme MD5
                    {"no-parallel", no_argument, NULL, NO_PARALLEL}, // Option longue pour ne pas utiliser de processus en parallèls
                    {"dry-run", no_argument, NULL, DRY_RUN}, // Option longue pour executer un test (pas de copie des fichiers)
                    {0, 0, 0, 0} // ligne obligatoire pour getopt_long
            };

            while ((opt = getopt_long(argc, argv, "n:vh", long_options, NULL)) != -1) { // Verifie qu'il reste des arguments à analyser
                switch (opt) { // en fonction des options
                    case 'n': // -n <nombre> : nombre de processus
                        the_config->processes_count = atoi(optarg);
                        break;
                    case 'v': // verbeux
                        the_config->is_verbose = true;
                        break;
                    case 'h': // fonction d'aide
                        display_help("lp25-backup");
                        break;
                    case DRY_RUN:
                        the_config->is_dry_run = true;
                        break;
                    case DATE_SIZE_ONLY:
                        the_config->uses_md5 = false;
                        break;
                    case NO_PARALLEL:
                        the_config->is_parallel = false;
                        break;
                    default: // Cas ou une des options ne correspond pas aux options possibles (getopt_long retourne quelque chose qui ne rentre dans aucun cas)
                        if (the_config->is_verbose == true) {
                            printf("Initialisation process failed\n");
                        }
                        display_help("lp25-backup"); // Affichage de l'aide
                        return -1; // Fin de la fonction avec erreur
                }
            }
        }
        if (the_config->is_verbose == true) {
            printf("Initialisation process is a success\n");
        }
        return 0; // Execution finie et pas d'erreur rencontrées
    }
}
