#include "file-properties.h"

#include <sys/stat.h>
#include <dirent.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "defines.h"
#include <fcntl.h>
#include <stdio.h>
#include "utility.h"

/*!
 * @brief get_file_stats gets all of the required information for a file (inc. directories)
 * @param the files list entry
 * You must get:
 * - for files:
 *   - mode (permissions)
 *   - mtime (in nanoseconds)
 *   - size
 *   - entry type (FICHIER)
 *   - MD5 sum
 * - for directories:
 *   - mode
 *   - entry type (DOSSIER)
 * @return -1 in case of error, 0 else
 */
int get_file_stats(files_list_entry_t *entry) {
    struct stat buffer_type;

    files_list_entry_t *pointeur = entry;                  //Définition d'un pointeur vers l'élément de la liste chaînée actuellement traité par la boucle while

    while (pointeur != NULL) {
        stat(pointeur->path_and_name, &buffer_type);

        if (&buffer_type != NULL) {                         //Si erreur avec le fichier dans le buffer
            printf("Error getting file stats");
            return -1;
        } else {
            if (S_ISREG(buffer_type.st_mode)) {             //Si le fichier est un fichier ordinaire
                // Type du fichier
                pointeur->entry_type = FICHIER;

                //Métadonnées fichier
                pointeur->mtime.tv_nsec = buffer_type.st_mtim.tv_nsec;
                pointeur->size = buffer_type.st_size;

                //Permissions fichier
                pointeur->mode = buffer_type.st_mode & 0777;

                // Somme MD5 fichier
                compute_file_md5(pointeur);


            } else if (S_ISDIR(buffer_type.st_mode)) {              //Si le fichier est un répertoire
                // Mode pour les dossiers
                pointeur->entry_type = DOSSIER;

                //Permissions répertoire
                pointeur->mode = buffer_type.st_mode & 0777;


            } else {
                //Fichier qui n'est ni un fichier régulier ni un répertoire
                printf("Unsupported file type\n");
                return -1;
            }
        }

        //Passage à l'élément suivant
        pointeur = pointeur->next;
    }

        return 0;
}


/*!
 * @brief compute_file_md5 computes a file's MD5 sum
 * @param the pointer to the files list entry
 * @return -1 in case of error, 0 else
 * Use libcrypto functions from openssl/evp.h
 */
int compute_file_md5(files_list_entry_t *entry) {
    FILE *file = fopen(entry->path_and_name, "rb");
    if (file == NULL) {
        printf("Error opening file for MD5 calculation");
        return -1;
    }

    EVP_MD_CTX *mdctx;
    const EVP_MD *md;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    md = EVP_md5();
    mdctx = EVP_MD_CTX_new();

    if (mdctx == NULL) {
        printf("Error creating MD5 context");
        fclose(file);
        return -1;
    }

    EVP_DigestInit_ex(mdctx, md, NULL);

    char buffer[1024];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        EVP_DigestUpdate(mdctx, buffer, bytes);
    }

    EVP_DigestFinal_ex(mdctx, md_value, &md_len);

    EVP_MD_CTX_free(mdctx);
    fclose(file);

    // Convertir la somme MD5 en format hexadécimal
    for (unsigned int i = 0; i < md_len; i++) {
        sprintf(&md5sum[i * 2], "%02x", md_value[i]);
    }
    md5sum[md_len * 2] = '\0';
}


/*!
 * @brief directory_exists tests the existence of a directory
 * @param path_to_dir a string with the path to the directory
 * @return true if directory exists, false else
 */
bool directory_exists(char *path_to_dir) {
    //Ouverture dossier
    DIR *directory = opendir(path_to_dir);

    if (directory != NULL) {
        //Le répertoire existe bien

        //Fermeture du répertoire (à ne pas oublier !)
        closedir(directory);

        return true;
    } else {
        //Le répertoire n'existe pas

        //Fermeture du répertoire (à ne pas oublier !)
        closedir(directory);

        return false;
    }
}


/*!
 * @brief is_directory_writable tests if a directory is writable
 * @param path_to_dir the path to the directory to test
 * @return true if dir is writable, false else
 * Hint: try to open a file in write mode in the target directory.
 */
bool is_directory_writable(char *path_to_dir) {
    //Ouverture du répertoire
    DIR *directory = opendir(path_to_dir);

    //Définition du fichier à ouvrir
    struct dirent *entry;
    char chemin_abs_fichier [255];

    bool sur_fichier_exploitable = false;
    while (! sur_fichier_exploitable) {
        //Enregistrement de 1 fichier
        entry = readdir(directory);

        // Ignorer les répertoires spéciaux "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construire le chemin complet du fichier
        snprintf(chemin_abs_fichier, sizeof(chemin_abs_fichier), "%s/%s", path_to_dir, entry->d_name);

        //Obtenir les informations sur le fichier
        struct stat file_stat;
        if (stat(chemin_abs_fichier, &file_stat) != 0) {
            printf("Erreur lors de la récupération des informations sur le fichier");
            continue;
        }

        // Vérifier si le fichier est bien un fichier ordinaire
        if (! S_ISREG(file_stat.st_mode)) {
            continue;
        } else {
            sur_fichier_exploitable = true;
        }
    }

    // Ouvrir ce fichier
    FILE *file = fopen(chemin_abs_fichier, "r");

    // Vérification de si l'ouverture a réussi
    if (file == NULL) {
        //Le fichier n'est pas rédigeable, donc le répertoire n'est pas en mode writable

        //Fermeture du ficher (à ne pas oublier !)
        fclose(file);
        //Fermeture du répertoire (à ne pas oublier aussi !)
        closedir(directory);

        return false;
    } else {
        //Le fichier est rédigeable, donc le répertoire est bien en mode writable

        //Fermeture du ficher (à ne pas oublier !)
        fclose(file);
        //Fermeture du répertoire (à ne pas oublier aussi !)
        closedir(directory);

        return true;
    }
}
