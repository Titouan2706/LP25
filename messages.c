#include <messages.h>
#include <sys/msg.h>
#include <string.h>
#include <stdio.h>

// Functions in this file are required for inter processes communication

/*!
 * @brief send_file_entry sends a file entry, with a given command code
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @param cmd_code is the cmd code to process the entry.
 * @return the result of the msgsnd function
 * Used by the specialized functions send_analyze*
 */
int send_file_entry(int msg_queue, int recipient, files_list_entry_t *file_entry, int cmd_code) {

    //Créer message
    any_message_t message;
    message.list_entry.mtype = recipient;
    message.list_entry.op_code = cmd_code;
    memcpy(&message.list_entry.payload, file_entry, sizeof(files_list_entry_t));
    message.list_entry.reply_to=msg_queue;
    //Envoyer message
    int result = msgsnd(msg_queue, &message, sizeof(any_message_t) - sizeof(long), 0);
    //Si erreur
    if (result == -1) {
        perror("msgsnd failed");
        return -1;
    }
    return result;

}

/*!
 * @brief send_analyze_dir_command sends a command to analyze a directory
 * @param msg_queue is the id of the MQ used to send the command
 * @param recipient is the recipient of the message (mtype)
 * @param target_dir is a string containing the path to the directory to analyze
 * @return the result of msgsnd
 */
int send_analyze_dir_command(int msg_queue, int recipient, char *target_dir) {
    analyze_dir_command_t command;
    command.mtype = recipient;
    strcpy(command.target, target_dir);
    command.op_code=COMMAND_CODE_ANALYZE_DIR;
    return msgsnd(msg_queue, &command, sizeof(analyze_dir_command_t) - sizeof(long), 0); 
}

// The 3 following functions are one-liners

/*!
 * @brief send_analyze_file_command sends a file entry to be analyzed
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_analyze_file_command(int msg_queue, int recipient, files_list_entry_t *file_entry) {
    return send_file_entry(msg_queue, recipient, file_entry, COMMAND_CODE_ANALYZE_FILE);
}

/*!
 * @brief send_analyze_file_response sends a file entry after analyze
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_analyze_file_response(int msg_queue, int recipient, files_list_entry_t *file_entry) {
    return send_file_entry(msg_queue, recipient, file_entry, COMMAND_CODE_FILE_ANALYZED);
}

/*!
 * @brief send_files_list_element sends a files list entry from a complete files list
 * @param msg_queue the MQ identifier through which to send the entry
 * @param recipient is the id of the recipient (as specified by mtype)
 * @param file_entry is a pointer to the entry to send (must be copied)
 * @param S_or_D a caractere to now if it's the source or destination list
 * @return the result of the send_file_entry function
 * Calls send_file_entry function
 */
int send_files_list_element(int msg_queue, int recipient, files_list_entry_t *file_entry,char S_or_D) {
    if(S_or_D=='S') {
        return send_file_entry(msg_queue,recipient,file_entry,COMMAND_CODE_FILE_ENTRY_FOR_SOURCE);
    }else{
     return send_file_entry(msg_queue,recipient,file_entry,COMMAND_CODE_FILE_ENTRY_FOR_DESTINATION);
    }
}

/*!
 * @brief send_list_end sends the end of list message to the main process
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @param S_or_D a caractere to now if it's the source or destination list
 * @return the result of msgsnd
 */
int send_list_end(int msg_queue, int recipient,char S_or_D) {
    //Créer message fin de liste
    any_message_t msg;
    msg.list_entry.mtype = recipient;
    if(S_or_D=='S'){
        msg.list_entry.op_code=COMMAND_CODE_LIST_COMPLETE_FOR_SOURCE;
    }else{
        msg.list_entry.op_code=COMMAND_CODE_LIST_COMPLETE_FOR_DESTINATION;
    }
    //Envoyer message
    return msgsnd(msg_queue, &msg, sizeof(files_list_entry_transmit_t) - sizeof(long), 0);
}

/*!
 * @brief send_terminate_command sends a terminate command to a child process so it stops
 * @param msg_queue is the MQ id used to send the command
 * @param recipient is the target of the terminate command
 * @return the result of msgsnd
 */
int send_terminate_command(int msg_queue, int recipient) {
    //Créer message
    any_message_t msg;
    msg.simple_command.mtype = recipient;
    msg.simple_command.message = COMMAND_CODE_TERMINATE;
    //Envoyer message
    return msgsnd(msg_queue, &msg, sizeof(simple_command_t) - sizeof(long), 0);
}

/*!
 * @brief send_terminate_confirm sends a terminate confirmation from a child process to the requesting parent.
 * @param msg_queue is the id of the MQ used to send the message
 * @param recipient is the destination of the message
 * @return the result of msgsnd
 */
int send_terminate_confirm(int msg_queue, int recipient) {
    //Créer message pour confirmation
    any_message_t msg;
    msg.simple_command.mtype = recipient;
    msg.simple_command.message = COMMAND_CODE_TERMINATE_OK;
    //Envoyer message
    return msgsnd(msg_queue, &msg, sizeof(simple_command_t) - sizeof(long), 0);
}
