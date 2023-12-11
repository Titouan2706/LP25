// THOMAS S

#include <configuration.h>
#include <stddef.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

typedef enum {DATE_SIZE_ONLY, NO_PARALLEL} long_opt_values;

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
    the_config->is_parallel = 1; // TRUE
    the_config->is_dry_run = 0; // FALSE
    the_config->is_verbose = 0; // FALSE
    the_config->uses_md5 = 1; // TRUE
    the_config->processes_count = 1; // A DEFINIR LE NOMBRE
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
        return -1;
    } else {
        if (argc != 3) {
            for (int i = 3; i <= argc; ++i) { // Parcours les autres arguments pour les options
                if (strcmp(argv[i], "--date-size-only") == 0) {
                    the_config->uses_md5 = 0; // On utilise pas la somme md5
                } else if (strcmp(argv[i], "--no-parallel") == 0) {
                    the_config->is_parallel = 0; // On utilise un seul processus pour tout le programme
                } else if (strcmp(argv[i], "-v") == 0) {
                    the_config->is_verbose = 1; // On affiche ce que le programme fait
                } else if (strcmp(argv[i], "--dry-run") == 0) {
                    the_config->is_dry_run = 1; // On ne copie pas les fichiers
                } else {
                    // cas argv[i] = "-n <nombre>" a faire
                    return -1; // Un des arguments n'est pas correct
                }
            }
        } else {
            return 0; // Pas de problèmes rencontrés
        }
    }
}
