#include <files-list.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

/*!
 * @brief clear_files_list clears a files list
 * @param list is a pointer to the list to be cleared
 * This function is provided, you don't need to implement nor modify it
 */
void clear_files_list(files_list_t *list) {
    while (list->head) {
        files_list_entry_t *tmp = list->head;
        list->head = tmp->next;
        free(tmp);
    }
}

/*!
 *  @brief add_file_entry adds a new file to the files list.
 *  It adds the file in an ordered manner (strcmp) and fills its properties
 *  by calling stat on the file.
 *  Il the file already exists, it does nothing and returns 0
 *  @param list the list to add the file entry into
 *  @param file_path the full path (from the root of the considered tree) of the file
 *  @return a pointer to the added element if success, NULL else (out of memory)
 */
files_list_entry_t *add_file_entry(files_list_t *list, char *file_path) {
    files_list_entry_t *new_entry = malloc(sizeof(files_list_entry_t)); // Créer une nouvelle entrée temporaire (parcours de la liste)
    if (new_entry == NULL) {
        return NULL; // Erreur d'allocation donc sortie de fonction
    } else {
        files_list_entry_t *temp = list->head; //
        while (temp != NULL && strcmp(temp->path_and_name, file_path) <= 0) { // Cherche la position correcte dans la liste
            temp = temp->next; // Incrémentation
        }

        if (temp == NULL) { // Fin de la liste atteinte
            new_entry->prev = list->tail; // Car dernier élément
            new_entry->next = NULL; // Pas de suivante car fin de liste
            if (list->head == NULL) { // Verifie si liste vide (ajouter en tête)
                list->head = new_entry; // Ajouter en tête
            } else if (list->tail != NULL) { // Si la queue de liste n'est pas vide
                list->tail->next = new_entry; // Ajouter en queue
            } else {
                list->tail = new_entry; // Si la queue de liste est vide
            }

        } else { // Ajouter entre deux éléments
            new_entry->prev = temp->prev; // gestion des listes
            new_entry->next = temp;
            if (temp->prev != NULL) {
                temp->prev->next = new_entry;
            } else {
                list->head = new_entry; // Ajouter en tête si pas de précédent
            }
            temp->prev = new_entry;
        }
    }
    return new_entry; // operation réussie
}

/*!
 * @brief add_entry_to_tail adds an entry directly to the tail of the list
 * It supposes that the entries are provided already ordered, e.g. when a lister process sends its list's
 * elements to the main process.
 * @param list is a pointer to the list to which to add the element
 * @param entry is a pointer to the entry to add. The list becomes owner of the entry.
 * @return 0 in case of success, -1 else
 */
int add_entry_to_tail(files_list_t *list, files_list_entry_t *entry) {
    if (list == NULL || entry == NULL) { // Verifie validité des arguments
        return -1; // Si paramètres invalides
    } else {
        if (list->tail == NULL) { // Pas d'éléments assignés en queue
            list->tail = entry; // Ajout de l'élément en queue
        } else {
            entry->prev = list->tail; // Ajout du chemin vers le précédent
            list->tail = entry; // Ajout du chemin vers la queue
            list->tail->next = NULL; // Pas de prochain éléments
        }

        return 0; // Opération réussie
    }
}

/*!
 *  @brief find_entry_by_name looks up for a file in a list
 *  The function uses the ordering of the entries to interrupt its search
 *  @param list the list to look into
 *  @param file_path the full path of the file to look for
 *  @param start_of_src the position of the name of the file in the source directory (removing the source path)
 *  @param start_of_dest the position of the name of the file in the destination dir (removing the dest path)
 *  @return a pointer to the element found, NULL if none were found.
 */
files_list_entry_t *find_entry_by_name(files_list_t *list, char *file_path, size_t start_of_src, size_t start_of_dest) {
    if (list == NULL || file_path == NULL) { // Vérifie la validité des paramètres
        return NULL; // Si les paramètres sont invalides
    }
    size_t file_path_length = strlen(file_path); // Longueur du chemin d'accès
    files_list_entry_t *temp = list->head; // Element temporaire pour le parcours
    while (temp != NULL) { // Parcours de la liste
        if (strncmp(temp->path_and_name + start_of_src, file_path + start_of_dest, file_path_length - start_of_dest) == 0) { // Compare les noms des chemins
            return temp; // Entrée trouvée
        }
        temp = temp->next; // Si la comparaison retourne un résultat pas concluant, passe à la prochaine itération
    }
    return NULL; // Entrée pas trouvée
}

/*!
 * @brief display_files_list displays a files list
 * @param list is the pointer to the list to be displayed
 * This function is already provided complete.
 */
void display_files_list(files_list_t *list) {
    if (!list)
        return;
    
    for (files_list_entry_t *cursor=list->head; cursor!=NULL; cursor=cursor->next) {
        printf("%s\n", cursor->path_and_name);
    }
}

/*!
 * @brief display_files_list_reversed displays a files list from the end to the beginning
 * @param list is the pointer to the list to be displayed
 * This function is already provided complete.
 */
void display_files_list_reversed(files_list_t *list) {
    if (!list)
        return;
    
    for (files_list_entry_t *cursor=list->tail; cursor!=NULL; cursor=cursor->prev) {
        printf("%s\n", cursor->path_and_name);
    }
}
