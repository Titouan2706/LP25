#include <defines.h>
#include <string.h>

/*!
 * @brief concat_path concatenates suffix to prefix into result
 * It checks if prefix ends by / and adds this token if necessary
 * It also checks that result will fit into PATH_SIZE length
 * @param result the result of the concatenation
 * @param prefix the first part of the resulting path
 * @param suffix the second part of the resulting path
 * @return a pointer to the resulting path, NULL when concatenation failed
 */
char *concat_path(char *result, char *prefix, char *suffix) {
   // On vérifie que les pointeurs d'entrée ne sont pas nuls
    if (result == NULL || prefix == NULL || suffix == NULL) {
        return NULL;
    }

    // On vérifie que la taille du préfixe est valide
    size_t prefix_len = strlen(prefix);
    if (prefix_len >= PATH_SIZE) {
        return NULL;  // La taille du préfixe est trop grande
    }

    // On copie le préfixe dans le résultat
    strcpy(result, prefix);

    // On ajoute '/' si nécessaire
    if (prefix_len > 0 && result[prefix_len - 1] != '/') {
        // On vérifie que le résultat a la taille nécessaire pour ajouter '/'
        if (prefix_len + 1 >= PATH_SIZE) {
            return NULL;  // Le chemin résultant serait trop long
        }

        // On ajoute '/'
        result[prefix_len] = '/';
        result[prefix_len + 1] = '\0';  // Terminer la chaîne après le '/'
    }

    // On concaténe le suffixe avec le résultat
    size_t suffix_len = strlen(suffix);
    if (prefix_len + suffix_len >= PATH_SIZE) { // Le chemin final est trop long
        return NULL;  
    }

    strcat(result, suffix);

    return result;
}
