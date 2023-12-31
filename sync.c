#include "sync.h"
#include <dirent.h>
#include <string.h>
#include "processes.h"
#include "utility.h"
#include "messages.h"
#include "file-properties.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/msg.h>

#include <stdio.h>

//Bibliothèques rajoutées pour pouvoir utiliser free() et utiliser les structures de mtime et utiliser l'attente wait()
#include <stdlib.h>
#include <utime.h>
#include <sys/wait.h>

#define MAX_PATH_SIZE 5121
//Calculée selon la taille des différents string : 1024+1+4096


/*!
 * @brief synchronize is the main function for synchronization
 * It will build the lists (source and destination), then make a third list with differences (???), and apply differences to the destination
 * It must adapt to the parallel or not operation of the program.
 * @param the_config is a pointer to the configuration
 * @param p_context is a pointer to the processes context
 */
void synchronize(configuration_t *the_config, process_context_t *p_context) {
    //1&2 - Construction listes source et destination
    files_list_t source_list, dest_list;

    if (! the_config->is_parallel) {
        //Si mode parallèle désactivé
        make_files_list(&source_list, the_config->source);
        make_files_list(&dest_list, the_config->destination);
    } else {
        //Si mode parallèle activé
        make_files_lists_parallel(&source_list, &dest_list, the_config, p_context->message_queue_id);
    }

    //3 - Vérification des différences : parcourir la liste des sources et synchroniser les fichiers

    files_list_entry_t *current_entry = source_list.head;
    files_list_entry_t *current_dest; //= dest_list.head;  Définie autrement après

    while (current_entry != NULL) {
        // Trouver l'entrée correspondante dans la liste de destination
        files_list_entry_t *current_dest = find_entry_by_name(&dest_list, current_entry->path_and_name, 0 ,0);

        if (current_dest == NULL) {
            //Si l'entrée n'existe pas dans la liste de destination, copier le fichier
            copy_entry_to_destination(current_entry, the_config);
        } else {
            //Comparer les attributs du fichier et recopier le fichier si besoin
            if (mismatch(current_entry, current_dest, the_config->uses_md5)) {
                copy_entry_to_destination(current_entry, the_config);
            }
        }

        current_entry = current_entry->next;
    }

    //Libération des listes créées
    if (&source_list != NULL) {
        // Libérer la mémoire pour chaque entrée de la liste
        files_list_entry_t *current = source_list.head;
        while (current != NULL) {
            files_list_entry_t *next = current->next;
            free(current);
            current = next;
        }

        // Libérer la mémoire pour la structure de la liste
        free(&source_list);
    }

    if (&dest_list != NULL) {
        // Libérer la mémoire pour chaque entrée de la liste
        files_list_entry_t *current = dest_list.head;
        while (current != NULL) {
            files_list_entry_t *next = current->next;
            free(current);
            current = next;
        }

        // Libérer la mémoire pour la structure de la liste
        free(&dest_list);
    }
}

/*!
 * @brief mismatch tests if two files with the same name (one in source, one in destination) are equal
 * @param lhd a files list entry from the source
 * @param rhd a files list entry from the destination
 * @param has_md5 a value to enable or disable MD5 sum check
 * @return true if both files are not equal, false else
 */
bool mismatch(files_list_entry_t *lhd, files_list_entry_t *rhd, bool has_md5) {
    if ((lhd == NULL) || (rhd == NULL)) {                               //Cas des structures vides
        printf("Error : the definition of source and/or destination file(s) is null.");
        return true;
    } else {
        int retour_erreurs;
        bool etat_comparaison = false;              //Variable qui contient le booléen du match entre les 2 fichiers (false = fichiers équivalents, true sinon)

        //Définition pointeurs éléments fichiers actuellement traités dans boucle while
        files_list_entry_t *pointeur_l = lhd;
        files_list_entry_t *pointeur_r = rhd;

        while ((pointeur_l != NULL) && (pointeur_r != NULL)) {
            //Vérification de chacun des attributs et comparaison + enregistrement dans etat_comparaison (booléen utilisant le connecteur "et" associé à l'addition).
            etat_comparaison += !(pointeur_l->entry_type == pointeur_r->entry_type);
            etat_comparaison += !(pointeur_l->mode == pointeur_r->mode);
            etat_comparaison += !(pointeur_l->mtime.tv_nsec == pointeur_r->mtime.tv_nsec);
            etat_comparaison += !(pointeur_l->size == pointeur_r->size);
            if (has_md5) {
                etat_comparaison += !(pointeur_l->md5sum == pointeur_r->md5sum);
            }

            return etat_comparaison;
        }
    }
    return false;
}


/*!
 * @brief make_files_list buils a files list in no parallel mode
 * @param list is a pointer to the list that will be built
 * @param target_path is the path whose files to list
 */
void make_files_list(files_list_t *list, char *target_path) {
    // Ouvrir le répertoire donné
    DIR *directory = open_dir(target_path);

    struct dirent *entry;

    // Parcourir chaque fichier du répertoire
    while ((entry = get_next_entry(directory)) != NULL) {
        // Construire le chemin complet du fichier
        char file_path[4096];
        //snprintf(file_path, sizeof(file_path), "%s/%s", target_path, entry->d_name);
        concat_path(file_path, target_path, entry->d_name);

        // Obtenir les stats sur le fichier
        struct stat file_stat;
        if (lstat(file_path, &file_stat) == -1) {
            printf("Erreur lors de l'obtention des informations sur le fichier.");
            continue;
        }

        // Vérifier si le fichier est un répertoire ou un fichier ordinaire
        if (! S_ISDIR(file_stat.st_mode) || ! S_ISREG(file_stat.st_mode)) {
            continue;
        }

        // Allouer de la mémoire pour une nouvelle entrée de la liste
        files_list_entry_t *new_entry = malloc(sizeof(files_list_entry_t));
        if (new_entry == NULL) {
            printf("Erreur lors de l'allocation de mémoire lors de la constitution de la liste de fichiers.");
            closedir(directory);
            return;
        }

        // Initialiser les champs de la nouvelle entrée
        strcpy(new_entry->path_and_name, file_path);
        if (new_entry->path_and_name == NULL) {
            printf("Erreur lors de l'allocation de mémoire.");
            free(new_entry);
            closedir(directory);
            return;
        }

        //Récupération informations sur le fichier
        if (get_file_stats(new_entry) == -1) {
            //Si erreur venant de get_file_stats, message erreur et free variable créée
            printf("Erreur lors de l'obtention des informations du fichier.");
            free(new_entry);
            continue;
        }

        // Ajouter la nouvelle entrée fichier à la liste doublement chaînée
        if (list == NULL) {
            //Si la liste est vide

            // Allouer de la mémoire pour la liste
            list = malloc(sizeof(files_list_t));
            if (list == NULL) {
                // La allocation a échoué, gérer l'erreur
                printf("Erreur d'allocation mémoire pour la liste.");
            }

            list->head = new_entry;
            list->tail = new_entry;
            new_entry->prev = NULL;
            new_entry->next = NULL;
        } else {
            new_entry->prev = list->tail;
            new_entry->next = NULL;
            list->tail->next = new_entry;
            list->tail = new_entry;
        }
    }

    // Fermer le répertoire
    closedir(directory);
}


/*!
 * @brief make_files_lists_parallel makes both (src and dest) files list with parallel processing
 * @param src_list is a pointer to the source list to build
 * @param dst_list is a pointer to the destination list to build
 * @param the_config is a pointer to the program configuration
 * @param msg_queue is the id of the MQ used for communication
 */
void make_files_lists_parallel(files_list_t *src_list, files_list_t *dst_list, configuration_t *the_config, int msg_queue) {

    any_message_t msg;
    //Envoie des messages au listeur
    if(the_config->is_verbose==true){
        printf("Envoie d'un message a chaque processus listeur\n");
    }
    send_analyze_dir_command(msg_queue,MSG_TYPE_TO_SOURCE_LISTER,the_config->source);
    send_analyze_dir_command(msg_queue,MSG_TYPE_TO_DESTINATION_LISTER,the_config->destination);

    bool list_source_complete= false;
    bool list_destination_complete=false;
    //Boucle de reception de message avec les fichiers analysés jusqu'à ce que les deux listes soit terminé
    if(the_config->is_verbose==true){
        printf("Début de la boucle de reception de message\n");
    }
    do{
        msgrcv(msg_queue,&msg, sizeof(any_message_t)- sizeof(long),MSG_TYPE_TO_MAIN,0);

        if (msg.list_entry.op_code==COMMAND_CODE_FILE_ENTRY_FOR_SOURCE){
            if(the_config->is_verbose==true){
                printf("Reception d'une entree de la source\n");
            }
            files_list_entry_t* new_entry= malloc(sizeof(files_list_entry_t));
            memcpy(new_entry,&msg.list_entry.payload, sizeof(files_list_entry_t));
            if(the_config->is_verbose==true){
                printf("Ajout de la nouvelle entree a la liste source\n");
            }
            add_entry_to_tail(src_list,new_entry);
        }else if (msg.list_entry.op_code==COMMAND_CODE_LIST_COMPLETE_FOR_SOURCE){
            if(the_config->is_verbose==true){
                printf("Reception du message de fin de liste pour la source\n");
            }
            list_source_complete=true;
        }else if (msg.list_entry.op_code==COMMAND_CODE_FILE_ENTRY_FOR_DESTINATION) {
            if(the_config->is_verbose==true){
                printf("Reception d'une entree de la destination\n");
            }
            files_list_entry_t *new_entry = malloc(sizeof(files_list_entry_t));
            memcpy(new_entry, &msg.list_entry.payload, sizeof(files_list_entry_t));
            if(the_config->is_verbose==true){
                printf("Ajout de la nouvelle entree a la liste destination\n");
            }
            add_entry_to_tail(dst_list, new_entry);
        }else if (msg.list_entry.op_code==COMMAND_CODE_LIST_COMPLETE_FOR_DESTINATION){
            if(the_config->is_verbose==true){
                printf("Reception du message de fin de lsite pour la destination\n");
            }
            list_destination_complete=true;
        }
    }while (list_source_complete==false || list_destination_complete==false);
    if(the_config->is_verbose==true){
        printf("Fin de la creation des listes en parallel\n");
    }
}


/*!
 * @brief copy_entry_to_destination copies a file from the source to the destination
 * It keeps access modes and mtime ( @see utimensat )
 * Pay attention to the path so that the prefixes are not repeated from the source to the destination
 * Use sendfile to copy the file, mkdir to create the directory
 */
void copy_entry_to_destination(files_list_entry_t *source_entry, configuration_t *the_config) {
    //Définition des chemins absolus de façon complète des fichiers
    char source_path[2048];
    char destination_path[4096];
    snprintf(source_path, MAX_PATH_SIZE, "%s/%s", the_config->source, source_entry->path_and_name);
    snprintf(destination_path, MAX_PATH_SIZE, "%s/%s", the_config->destination, source_entry->path_and_name);

    struct stat buffer_type;
    stat(source_entry->path_and_name, &buffer_type);
    if (&buffer_type != NULL) {                         //Si erreur avec le fichier dans le buffer
        printf("Erreur lors de l'obtention des stats du fichier.");
    }

    //Test type du fichier donné
    if (S_ISDIR(buffer_type.st_mode)) {              //Le fichier est un répertoire
        //Création d'un répertoire dans la destination
        int retour_mkdir = mkdir(destination_path, source_entry->mode);

        //Vérification de la bonne création
        if (retour_mkdir == -1) {
            printf("Erreur lors de la création du répertoire copie.");
            return;
        }

        //Réattribution des mêmes droits
        if (chmod(destination_path, buffer_type.st_mode) == -1) {
            printf("Erreur lors de l'attribution des permissions au répertoire en destination.\n");
            return;
        }

        if (chown(destination_path, buffer_type.st_uid, buffer_type.st_gid) == -1) {
            printf("Erreur lors de l'attribution du propriétaire et du groupe au répertoire en destination.\n");
            return;
        }

    } else if (S_ISREG(buffer_type.st_mode)) {       //Le fichier est un fichier ordinaire
        //Ouverture du fichier source
        FILE *source_file = fopen(source_path, "rb");
        if (source_file == NULL) {
            printf("Erreur à l'ouverture du fichier source.");
            return;
        }

        //Ouverture ou création du fichier dans la destination
        FILE *destination_file = fopen(destination_path, "wb");
        if (destination_file == NULL) {
            perror("Erreur lors de l'ouverture/la création du fichier dans la destination.");
            fclose(source_file);
            return;
        }

        //Copiage du contenu du fichier source dans le fichier de destination
        /*
        int caract;
        while ((caract = fgetc(source_file)) != EOF) {
            fputc(caract, destination_file);
        }
        */
        if (sendfile(destination_file, source_file, NULL, buffer_type.st_size) == -1) {
            printf("Erreur lors de la copie des données avec sendfile.\n");
        }


        //Modification du mtime dans la destination
        struct utimbuf utime_buf;
        utime_buf.actime = source_entry->mtime.tv_sec;  // Access time same as source
        utime_buf.modtime = source_entry->mtime.tv_sec; // Modification time same as source
        utime(destination_path, &utime_buf);

        //Modification des droits d'accès du fichier destination
        chmod(destination_path, source_entry->mode);

        //Fermeture des fichiers
        fclose(source_file);
        fclose(destination_file);

    } else {                                        //Erreur sur le type du fichier transmis
        printf("%s n'est ni un fichier ordinaire, ni un répertoire. Format non accepté.\n", source_entry->path_and_name);
        return;
    }
}


/*!
 * @brief make_list lists files in a location (it recurses in directories)
 * It doesn't get files properties, only a list of paths
 * This function is used by make_files_list and make_files_list_parallel
 * @param list is a pointer to the list that will be built
 * @param target is the target dir whose content must be listed
 */
 /*!
 * Cette fonction n'est pas utilisée car il est plus simple de traiter dans un seul tour de boucle while
 * la création du path du fichier ainsi que l'obtention de ces propriétés
 * qui seront enregistrés ensemble dans la nouvelle entrée de la liste de fichiers.
 * Le contenu de cette fonction est donc directement imbriquée dans make_files_list et make_files_list_parallel.
 */
void make_list(files_list_t *list, char *target) {
}


/*!
 * @brief open_dir opens a dir
 * @param path is the path to the dir
 * @return a pointer to a dir, NULL if it cannot be opened
 */
DIR *open_dir(char *path) {
    DIR *directory = opendir(path);
    if (directory == NULL) {
        //Erreur à l'ouverture du répertoire
        printf("Erreur lors de l'ouverture du répertoire.");
        return NULL;
    } else {
        return directory;
    }
}


/*!
 * @brief get_next_entry returns the next entry in an already opened dir
 * @param dir is a pointer to the dir (as a result of opendir, @see open_dir)
 * @return a struct dirent pointer to the next relevant entry, NULL if none found (use it to stop iterating)
 * Relevant entries are all regular files and dir, except . and ..
 */
struct dirent *get_next_entry(DIR *dir) {
    struct dirent *fichier_entree;
    struct stat buffer_type;


    do {
        fichier_entree = readdir(dir);

        //Si fin du répertoire
        if (fichier_entree == NULL) {
            return NULL;
        }

        stat(fichier_entree->d_name, &buffer_type);
        if (&buffer_type != NULL) {                         //Si erreur avec le fichier dans le buffer
            printf("Erreur lors de l'obtention des stats du fichier.");
            continue;
        }
    } while (strcmp(fichier_entree->d_name, ".") == 0 || strcmp(fichier_entree->d_name, "..") == 0 || (!(S_ISREG(buffer_type.st_mode)) && (!(S_ISDIR(buffer_type.st_mode)))));
    // Ignorer les répertoires spéciaux "." et ".." + Vérification du type du fichier dans la fonction appelante.
    //Si fichier non conforme, on relit une nouvelle entrée et on reprend ces informations.

    return fichier_entree;
}
