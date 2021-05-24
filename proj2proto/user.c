#include <stdio.h>
#include "list.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#define __user

long create_mbox_421(unsigned long);
long remove_mbox_421(unsigned long);
long send_msg_421(unsigned long, const unsigned char __user *, long);
long count_mbox_421(void);
long print_mbox_421(unsigned long);
long count_msg_421(unsigned long);
long reset_mbox_421(void);
long len_msg_421(unsigned long);
long list_mbox_421(unsigned long __user *, long );
long recv_msg_421(unsigned long, unsigned char __user *, long);
long peek_msg_421(unsigned long, unsigned char __user *, long);
unsigned long get_mbox_id(void);

struct mail{
    unsigned char *message;
    long len;
    struct list_head head_of_msgs;
};

struct mbox{
    struct list_head list;
    unsigned long ID;
    struct list_head mail_list;
    long num_of_msgs;
};

LIST_HEAD(head);
static long num_mboxes = 0;

int main(){
    
    unsigned long ID = get_mbox_id();
    unsigned long check = -1;
    if (ID == check){
        printf("That's a bad ID!\n");
    } else {
        long val = create_mbox_421(ID);
        printf("Mailbox added with ID %ld\n", val);
    }

    create_mbox_421(1);
    
    unsigned long ID_to_check = 1;
    unsigned char store_me[17] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x10, 0x11, 0x12, 
    0x13, 0x14, 0x15, 0x16, 0x17 };
    long size = 17;
    send_msg_421(ID_to_check, store_me, size);
    send_msg_421(ID_to_check, store_me, size);


    
    //len_msg_421(1);
    /*
    unsigned long store_me2[1];

    list_mbox_421(store_me2, 1);

    printf("ID found: %lu\n", store_me2[0]);
    
    print_mbox_421(69);
    
    count_msg_421(69);
    remove_mbox_421(ID_to_check);
    
    printf("Print mbox before recv\n");
    print_mbox_421(69);
    */
    printf("Before recieve:\n");
    long num_msg;
    num_msg = count_msg_421(1);
    printf("This mailbox has %lu messages\n", num_msg);

    unsigned char store_me3[0];
    //recv_msg_421(69, store_me3, 17);
    recv_msg_421(1, store_me3, 17);

    printf("After recieve\n");
    long num_msg2;
    num_msg2 = count_msg_421(1);
    printf("This mailbox has %lu messages\n", num_msg2);


    /*
    printf("Byte found: %02x\n", store_me3[16]);


    printf("Print mbox after revc\n");
    print_mbox_421(69);
    */

    reset_mbox_421();
    return 0;
}

//Main problem, trouble iterating through list when entering create_mbox_421
long create_mbox_421(unsigned long id){

    //Check if the ID is already in the mailbox linked list
    struct mbox *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            printf("This mailbox ID is already registered!\n");
            return 0; //SHOULD RETURN AN ERROR CODE
        }
    }

    //Mailbox ID is not in the system, add to the linked list
    struct mbox *mailbox_to_add;
    mailbox_to_add=(struct mbox *)malloc(sizeof(struct mbox));
    mailbox_to_add->ID=id;
    
    //Initialize the linked list that stores messages
    INIT_LIST_HEAD(&mailbox_to_add->mail_list);

    list_add_tail(&mailbox_to_add->list,&head);
    num_mboxes++;

    return id;
}

long remove_mbox_421(unsigned long id){
    if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }
    
    struct mbox *curr_mbox;
    struct list_head *ptr = NULL;
    struct list_head *tmp;
    unsigned long curr_id;
    list_for_each_safe(ptr, tmp, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        //Dont remove a mailbox that still has mail in it
        if (curr_id == id && list_empty(&curr_mbox->mail_list) == 1){
            list_del(ptr);
            free(curr_mbox);
            return id;
        }
    }
    printf("That mailbox isn't in the system or it has mail!\n");
    return 0; //SHOULD RETURN ERROR CODE
}

long count_mbox_421(void){
    return num_mboxes;
}

long send_msg_421(unsigned long id, const unsigned char __user *msg, long n){
    if(n < 0){
        printf("Cannot take in negative lengths.\n");
        return 0; //SHOULD BE AN ERROR VALUE
    }

    if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }

    struct mbox *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;

    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            struct mail *message_to_add;
            message_to_add = (struct mail *)malloc(sizeof(struct mail));
            message_to_add->message = malloc(n);

            memcpy(message_to_add->message, msg, n);
            message_to_add->len = n;

            if(list_empty(&curr_mbox->mail_list)==1){
                curr_mbox->num_of_msgs = 0;
            }

            list_add_tail(&message_to_add->head_of_msgs, &curr_mbox->mail_list);
            curr_mbox->num_of_msgs++;

            return 0;
        }
    }
    printf("That mailbox isn't in the system!\n");

    return 0; //SHOULD RETURN AN ERROR CODE
}

long print_mbox_421(unsigned long id){
    if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }
    
    struct mbox *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            struct mail *poe2; 
            long curr_msg_len;
            list_for_each_entry(poe2, &curr_mbox->mail_list, head_of_msgs){
                curr_msg_len = poe2->len;
                int i;

                for(i = 0; i < curr_msg_len; i++){
                    //Max 16 bytes per line
                    if(i != 0 && i%16==0){
                        printf("\n");
                    }
                    printf("%02x ", poe2->message[i]);
                }
                //Seperate messages
                printf("\n---\n");
            }
            printf("\n");
            return 0; //Successful return
        }
    }
    printf("Mailbox is not in the system!\n");
    return 0; //Should be error message
}

long count_msg_421(unsigned long id){
    if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }
    
    struct mbox *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            long num_msgs;
            num_msgs = curr_mbox->num_of_msgs;
            return num_msgs;
        }
    }
    printf("Mailbox is not in the system!\n");
    return 0; //Should be error message
}

long reset_mbox_421(void){
    //Check if there are any mailboxes in the system
    if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }

    struct mbox *curr_mbox;
    struct mbox *temp_mbox;

    list_for_each_entry_safe(curr_mbox, temp_mbox, &head, list){
        struct mail *curr_mail;
        struct mail *temp_mail;

        list_for_each_entry_safe(curr_mail, temp_mail, &curr_mbox->mail_list, head_of_msgs){
            list_del(&curr_mail->head_of_msgs);
            //Make sure to free message aswell
            free(curr_mail->message);
            free(curr_mail);
        }

        list_del(&curr_mbox->list);
        free(curr_mbox);
    }

    return 0;
}

long len_msg_421(unsigned long id){
    if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }
    
    struct mbox *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            if (list_empty(&curr_mbox->mail_list)==1){
                printf("This mailbox is empty!\n");
                return 0; //Should be an error message
            } else {
                struct mail *poe2; 
                long curr_msg_len;
                list_for_each_entry(poe2, &curr_mbox->mail_list, head_of_msgs){
                    curr_msg_len = poe2->len;
                    printf("This is the next message length: %ld\n", curr_msg_len);
                    //Only need first message so you can return here
                    return curr_msg_len;
                }
            }
        }
    }
    printf("That mailbox isn't in the system!\n");
    
    return 0;
}

long list_mbox_421(unsigned long __user *mbxes, long k){
    struct mbox *curr_mbox;
    struct list_head *ptr;
    long count = 0;

    list_for_each(ptr, &head){
        //Accounting for the case when there 
        //are more mailboxes than the user wants
        if(count == k){
            return count;
        }
        curr_mbox = list_entry(ptr, struct mbox, list);
        mbxes[count] = curr_mbox->ID;
        count++;
    }
    return count;
}

long recv_msg_421(unsigned long id, unsigned char __user *msg, long n){
     if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }
    
    struct mbox *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            if (list_empty(&curr_mbox->mail_list)==1){
                printf("This mailbox is empty!\n");
                return 0; //Should be an error message
            } else {
                struct mail *poe2; 
                list_for_each_entry(poe2, &curr_mbox->mail_list, head_of_msgs){
                    msg = malloc(n);

                    long len_to_compare = poe2->len;

                    memcpy(msg, poe2->message, n);
                    free(msg);
                    
                    list_del(&poe2->head_of_msgs);
                    //Make sure to free message aswell
                    free(poe2->message);
                    free(poe2);
                    
                    curr_mbox->num_of_msgs -= 1;

                    if(len_to_compare < n){
                        return len_to_compare;
                    }
                    return n; //succeful return
                }
            }
        }
    }
    return 0; //Should return error message
}

long peek_msg_421(unsigned long id, unsigned char __user *msg, long n){
    if (list_empty(&head) == 1){
        printf("There are no mailboxes in the system!\n");
        return 0; //Should be an error code
    }
    
    struct mbox *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, struct mbox, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            if (list_empty(&curr_mbox->mail_list)==1){
                printf("This mailbox is empty!\n");
                return 0; //Should be an error message
            } else {
                struct mail *poe2; 
                list_for_each_entry(poe2, &curr_mbox->mail_list, head_of_msgs){
                    msg = malloc(n);

                    long len_to_compare = poe2->len;

                    memcpy(msg, poe2->message, n);
                    free(msg);
                    
                    if(len_to_compare < n){
                        return len_to_compare;
                    }
                    
                    return n; //succeful return
                }
            }
        }
    }
    return 0; //Should return error message
}

//Function to get the desired ID
unsigned long get_mbox_id(){
    unsigned long potential_id;
    printf("Enter a mailbox ID: ");
    int check = scanf("%ld", &potential_id);
    if (check == 0){
        return -1;
    } else {
        return potential_id;
    }
    return 0;
}
