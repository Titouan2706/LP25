#include "processes.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <stdio.h>
#include <messages.h>
#include <file-properties.h>
#include <sync.h>
#include <string.h>
#include <errno.h>

/*!
 * @brief prepare prepares (only when parallel is enabled) the processes used for the synchronization.
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the program processes context
 * @return 0 if all went good, -1 else
 */
int prepare(configuration_t *the_config, process_context_t *p_context) {
    if (the_config!=NULL && the_config->is_parallel==true){
        if(the_config->is_verbose==true){
            printf("Creation de la MSQ_Key");
        }
        p_context->shared_key= ftok("lp25-backup",49);
        if (p_context->shared_key==-1){
            printf("Erreur avec la MQKey");
            return -1;
        }
        if(the_config->is_verbose==true){
            printf("Ouverture de la MSQ");
        }
        p_context->message_queue_id=msgget(p_context->shared_key,0666 |IPC_CREAT);
        if (p_context->message_queue_id==-1){
            printf("Erreur lors de la création de la MsgQueue");
            return -1;
        }
        p_context->main_process_pid=getpid();
        p_context->processes_count=0;
        if(the_config->is_verbose==true){
            printf("Parametrage processus lister_source + mise en place du processus");
        }
        lister_configuration_t parametres_lister_source;
        parametres_lister_source.my_recipient_id=MSG_TYPE_TO_SOURCE_ANALYZERS;
        parametres_lister_source.my_receiver_id=MSG_TYPE_TO_SOURCE_LISTER;
        parametres_lister_source.analyzers_count=(the_config->processes_count);
        parametres_lister_source.mq_key=p_context->shared_key;
        p_context->source_lister_pid= make_process(p_context,lister_process_loop, &parametres_lister_source);

        if(the_config->is_verbose==true){
            printf("Parametrage processus analyseur_source + mise en place du/des processus");
        }
        analyzer_configuration_t parametres_analyseur_source;
        parametres_analyseur_source.my_recipient_id=MSG_TYPE_TO_SOURCE_LISTER;
        parametres_analyseur_source.my_receiver_id=MSG_TYPE_TO_SOURCE_ANALYZERS;
        parametres_analyseur_source.mq_key=p_context->shared_key;
        parametres_analyseur_source.use_md5=the_config->uses_md5;
        p_context->source_analyzers_pids=malloc(sizeof(pid_t)*the_config->processes_count);
        for (int i = 0; i < the_config->processes_count; ++i) {
                p_context->source_analyzers_pids[i]= make_process(p_context,analyzer_process_loop,&parametres_analyseur_source);
        }
        if(the_config->is_verbose==true){
            printf("Parametrage processus listeur_destination + mise en place du processus");
        }
        lister_configuration_t parametres_lister_destinataion;
        parametres_lister_destinataion.my_recipient_id=MSG_TYPE_TO_DESTINATION_ANALYZERS;
        parametres_lister_destinataion.my_receiver_id=MSG_TYPE_TO_DESTINATION_LISTER;
        parametres_lister_destinataion.analyzers_count=(the_config->processes_count);
        parametres_lister_destinataion.mq_key=p_context->shared_key;
        p_context->destination_lister_pid= make_process(p_context,lister_process_loop, &parametres_lister_destinataion);

        if(the_config->is_verbose==true){
            printf("Parametrage processus analyseur_sourcec + mise en place du/des processus");
        }
        analyzer_configuration_t parametres_analyseur_destination;
        parametres_analyseur_destination.my_recipient_id=MSG_TYPE_TO_DESTINATION_LISTER;
        parametres_analyseur_destination.my_receiver_id=MSG_TYPE_TO_DESTINATION_ANALYZERS;
        parametres_analyseur_destination.mq_key=p_context->shared_key;
        parametres_analyseur_destination.use_md5=the_config->uses_md5;
        p_context->destination_analyzers_pids=malloc(sizeof(pid_t)*the_config->processes_count);
        for (int i = 0; i < the_config->processes_count; ++i) {
            p_context->destination_analyzers_pids[i]= make_process(p_context,analyzer_process_loop,&parametres_analyseur_destination);
        }

    }
    return 0;
}

/*!
 * @brief make_process creates a process and returns its PID to the parent
 * @param p_context is a pointer to the processes context
 * @param func is the function executed by the new process
 * @param parameters is a pointer to the parameters of func
 * @return the PID of the child process (it never returns in the child process)
 */
int make_process(process_context_t *p_context, process_loop_t func, void *parameters) {
    pid_t  child_pid = fork();
    if (child_pid==0){
        func(parameters);
    }else{
        ++p_context->processes_count;
        return child_pid;
    }
}

/*!
 * @brief lister_process_loop is the lister process function (@see make_process)
 * @param parameters is a pointer to its parameters, to be cast to a lister_configuration_t
 */
void lister_process_loop(void *parameters) {
    files_list_t new_list;
    new_list.head=NULL;
    new_list.tail=NULL;
    any_message_t message;
    lister_configuration_t* configuration= (lister_configuration_t*) parameters;
    files_list_entry_t * file_without_detail;
    files_list_entry_t * file_with_detail;
    int msq_id=msgget(configuration->mq_key,0666);
    long p_used;

    do{
        if (msgrcv(msq_id,&message, sizeof(any_message_t)- sizeof(long),configuration->my_receiver_id,0)!=-1){
            if (message.analyze_file_command.op_code==COMMAND_CODE_ANALYZE_DIR){
                make_list(&new_list,message.analyze_dir_command.target);
                file_without_detail= new_list.head;
                file_with_detail= new_list.head;
                while (file_without_detail!=NULL) {
                    while (p_used < configuration->analyzers_count && file_without_detail != NULL) {
                        send_analyze_file_command(msq_id,configuration->my_recipient_id,file_without_detail);
                        file_without_detail=file_without_detail->next;
                        ++p_used;
                    }
                    while (p_used>0){
                        msgrcv(msq_id, &message,sizeof(any_message_t)- sizeof(long),configuration->my_receiver_id,0);
                        memcpy(file_with_detail, &message.analyze_file_command.payload, sizeof(files_list_entry_t));
                        --p_used;
                        file_with_detail=file_with_detail->next;
                    }
                }
                while (file_with_detail->prev!=NULL){
                    file_with_detail=file_with_detail->prev;
                }

                while (file_with_detail!=NULL){
                    if (configuration->my_receiver_id ==MSG_TYPE_TO_SOURCE_LISTER){
                        send_files_list_element(msq_id,MSG_TYPE_TO_MAIN,file_with_detail,'S');
                    }else{
                        send_files_list_element(msq_id,MSG_TYPE_TO_MAIN,file_with_detail,'D');
                    }
                    file_with_detail=file_with_detail->next;
                }
                if (configuration->my_receiver_id==MSG_TYPE_TO_SOURCE_LISTER){
                    send_list_end(msq_id,MSG_TYPE_TO_MAIN,'S');
                }else{
                    send_list_end(msq_id,MSG_TYPE_TO_MAIN,'D');
                }
            }
        }
    }while (message.simple_command.message!=COMMAND_CODE_TERMINATE);
    send_terminate_confirm(msq_id,MSG_TYPE_TO_MAIN);
    exit(EXIT_SUCCESS);
}

/*!
 * @brief analyzer_process_loop is the analyzer process function
 * @param parameters is a pointer to its parameters, to be cast to an analyzer_configuration_t
 */
void analyzer_process_loop(void *parameters) {
    analyzer_configuration_t* configuration=(analyzer_configuration_t*) parameters;
    any_message_t message;
    int msq_id=msgget(configuration->mq_key,0666);
    do{
        if (msgrcv(msq_id, &message, sizeof(any_message_t)- sizeof(long),configuration->my_receiver_id,0)!=-1){
            if (message.analyze_file_command.op_code==COMMAND_CODE_ANALYZE_FILE){
                get_file_stats(&message.analyze_file_command.payload);
                send_analyze_file_command(msq_id,configuration->my_recipient_id,&message.analyze_file_command.payload);
            }
        }
    }while (message.simple_command.message!= COMMAND_CODE_TERMINATE);
    send_terminate_confirm(msq_id,MSG_TYPE_TO_MAIN);
}

/*!
 * @brief clean_processes cleans the processes by sending them a terminate command and waiting to the confirmation
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the processes context
 */
void clean_processes(configuration_t *the_config, process_context_t *p_context) {
    if(the_config!=NULL && p_context!=NULL){
        if(the_config->is_parallel!=false){
            any_message_t message;
            long nbr_message=0;
            //Envoie des messages terminaux au processus lister
            send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_SOURCE_LISTER);
            send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_DESTINATION_LISTER);
            //Boucle pour envoie des messages terminaux à tous les processus analyseurs
            for (int i = 0; i < p_context->processes_count; ++i) {
                send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_SOURCE_LISTER);
                send_terminate_command(p_context->message_queue_id,MSG_TYPE_TO_DESTINATION_LISTER);
            }
            //Attente de reception de tous les messages de confirmation de fermeture
            while (nbr_message<(p_context->processes_count*2)+2){
                if(msgrcv(p_context->message_queue_id,&message, sizeof(any_message_t)- sizeof(long),MSG_TYPE_TO_MAIN,0)!=-1){
                    ++nbr_message;
                }
            }
            //Liberation de la mémoire
            free(p_context->source_analyzers_pids);
            free(p_context->destination_analyzers_pids);
            //Fermeture de la MSQ
            msgctl(p_context->message_queue_id,IPC_RMID,NULL);
        }
    }else{
        printf("Error for cleaning processes");
    }
}

