#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/list.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/rwsem.h>

typedef struct mail{
    unsigned char *message;
    long len;
    struct list_head head_of_msgs;
} mail_t;

typedef struct mbox{
    struct list_head list;
    unsigned long ID;
    struct list_head mail_list;
    long num_of_msgs;
    struct rw_semaphore mail_lock;
} mbox_t;

static LIST_HEAD(head);
static long num_mboxes = 0;
static DECLARE_RWSEM(lock);

SYSCALL_DEFINE1(create_mbox_421, unsigned long, id){
    mbox_t *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    mbox_t *mailbox_to_add;
    
    /*Check for root user*/
    if (current_cred()->uid.val != 0){
        return -EPERM;
    }

    /*Check if the ID is already in the mailbox linked list*/
    if (list_empty(&head) == 1){
        /*Skip, no need to iterate through mailbox list*/
    } else {
        down_read(&lock);
        list_for_each(ptr, &head){
            curr_mbox = list_entry(ptr, mbox_t, list);
            curr_id = curr_mbox->ID;
            if (curr_id == id){
                up_read(&lock);
                return -EEXIST; /*Mailbox already exists error*/
            }
        }
        up_read(&lock);
    }
    
    /*Mailbox ID is not in the system, add to the linked list*/
    mailbox_to_add=(mbox_t *)kmalloc(sizeof(mbox_t), GFP_KERNEL);
    if (mailbox_to_add == NULL){ /*Check if kernel is out of memory*/
        return -ENOMEM;
    }
    mailbox_to_add->ID=id;
    
    /*Initialize the linked list that stores messages*/
    INIT_LIST_HEAD(&mailbox_to_add->mail_list);

    /*Initialize the local lock for the mailbox*/
    init_rwsem(&mailbox_to_add->mail_lock);

    down_write(&lock);
    list_add_tail(&mailbox_to_add->list,&head); 
    up_write(&lock);

    num_mboxes++; 
    
    return 0; /*Successful Return*/
}

SYSCALL_DEFINE1(remove_mbox_421, unsigned long, id){
    mbox_t *curr_mbox;
    struct list_head *ptr = NULL;
    struct list_head *tmp;
    unsigned long curr_id;
   
    if (current_cred()->uid.val != 0){
        return -EPERM;
    }

    down_write(&lock);
    /*Find mailbox to delete*/
    list_for_each_safe(ptr, tmp, &head){
        curr_mbox = list_entry(ptr, mbox_t, list);
        curr_id = curr_mbox->ID;
        /*Dont remove a mailbox that still has mail in it*/
        if (curr_id == id){
            if (list_empty(&curr_mbox->mail_list) == 0){
                up_write(&lock);
                return -ENOTEMPTY;
            }
            
            list_del(ptr);
            up_write(&lock);
            num_mboxes -= 1;
            
            kfree(curr_mbox);
            
            return 0;
        } 
    }
    up_write(&lock);
    return -ENOENT; /*ID not found error*/
}

SYSCALL_DEFINE0(reset_mbox_421){
    mbox_t *curr_mbox;
    mbox_t *temp_mbox;
    mail_t *curr_mail;
    mail_t *temp_mail;
    
    if (current_cred()->uid.val != 0){
        return -EPERM;
    }

    down_write(&lock);
    list_for_each_entry_safe(curr_mbox, temp_mbox, &head, list){

        down_write(&curr_mbox->mail_lock);
        list_for_each_entry_safe(curr_mail, temp_mail, &curr_mbox->mail_list, head_of_msgs){
            list_del(&curr_mail->head_of_msgs);
            /*Make sure to free message aswell*/
            kfree(curr_mail->message);
            kfree(curr_mail);
        }
        up_write(&curr_mbox->mail_lock);

        list_del(&curr_mbox->list);
        kfree(curr_mbox);
    }

    up_write(&lock);
    num_mboxes = 0;
    
    return 0;
}

SYSCALL_DEFINE0(count_mbox_421){
    unsigned long count;
    down_read(&lock);
    count = num_mboxes;
    up_read(&lock);
    return count;
}

SYSCALL_DEFINE2(list_mbox_421, unsigned long __user *, mbxes, long, k){
    mbox_t *curr_mbox;
    struct list_head *ptr;
    long count = 0;
    unsigned long *temp;

    if(!access_ok(mbxes, k*sizeof(unsigned long))){
        return -EFAULT;
    }

    temp = (unsigned long *)kmalloc(k*sizeof(unsigned long), GFP_KERNEL);
    if(temp == NULL){
        return -ENOMEM;
    }

    down_read(&lock);
    list_for_each(ptr, &head){
        /*Accounting for the case when there are more mailboxes
        than the user wants and if the user wants more mboxes than
        there are in the system*/
        if(count == k || count == num_mboxes){
            up_read(&lock);
            if (copy_to_user(mbxes, temp, k*sizeof(unsigned long))){
                /*copy failed*/
                kfree(temp);
                return -EFAULT;
            } else {
                /*copy succeeded*/
                kfree(temp);
                return count;
            }
        }
        curr_mbox = list_entry(ptr, mbox_t, list);
        temp[count] = curr_mbox->ID;
        count++;
    }
    up_read(&lock);

    if (copy_to_user(mbxes, temp, k*sizeof(unsigned long))){
        /*failure*/
        kfree(temp);
        return -EFAULT;
    } else {
        /*success*/
        kfree(temp);
        return count;
    }
}

SYSCALL_DEFINE3(send_msg_421, unsigned long, id, const unsigned char __user*, msg, long, n){
    mbox_t *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    mail_t *message_to_add;
    
    if(n < 0){
        /*Cannot take in negative message lengths*/
        return -EFAULT;
    }

    if(!access_ok(msg, n)){
        return -EFAULT;
    }

    down_read(&lock);
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, mbox_t, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            message_to_add = (mail_t *)kmalloc(sizeof(mail_t), GFP_KERNEL);
            if(message_to_add == NULL){
                return -ENOMEM;
            }
            message_to_add->message = kmalloc(n, GFP_KERNEL);
            if(message_to_add->message == NULL){
                return -ENOMEM;
            }

            if(copy_from_user(message_to_add->message, msg, n)){
                /*copy failed*/
                up_read(&lock);
                return -EFAULT;
            } else {
                /*copy successful*/
                message_to_add->len = n;

                /*Initialize the message counter variable*/
                if(list_empty(&curr_mbox->mail_list)==1){
                    curr_mbox->num_of_msgs = 0;
                }

                down_write(&curr_mbox->mail_lock);
                list_add_tail(&message_to_add->head_of_msgs, &curr_mbox->mail_list);
                curr_mbox->num_of_msgs++;
                up_write(&curr_mbox->mail_lock);

                up_read(&lock);
                return n; /*Successful Return*/
            }
        }
    }
    
    up_read(&lock);
    return -ENOENT; /*ID not found error*/
}

SYSCALL_DEFINE3(recv_msg_421, unsigned long, id, unsigned char __user*, msg, long, n){
    mbox_t *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    mail_t *curr_mail;
    long len_to_compare;

    if (msg == NULL){
        return -EFAULT;
    }

    if(!access_ok(msg, n)){
        return -EFAULT;
    }

    down_read(&lock);
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, mbox_t, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            if (list_empty(&curr_mbox->mail_list)==1){
                up_read(&lock);
                return -ENOENT; /*Mailbox has no mail in it*/
            } else {
                down_write(&curr_mbox->mail_lock);
                list_for_each_entry(curr_mail, &curr_mbox->mail_list, head_of_msgs){
                    if (copy_to_user(msg,curr_mail->message,n)){
                        /*copy failure*/
                        up_write(&curr_mbox->mail_lock);
                        up_read(&lock);
                        return -EFAULT;
                    } else {
                        /*copy succeded*/
                        len_to_compare = curr_mail->len;                  

                        /*Recv delete the message*/
                        list_del(&curr_mail->head_of_msgs);
                        curr_mbox->num_of_msgs -= 1;

                        up_write(&curr_mbox->mail_lock);

                        /*Make sure to free message aswell*/
                        kfree(curr_mail->message);
                        kfree(curr_mail);

                        if(len_to_compare < n){
                            up_read(&lock);
                            return len_to_compare;
                        }
                        up_read(&lock);
                        return n; /*succeful return*/
                    }
                }
            }
        }
    }
    up_read(&lock);
    return -ENOENT; /*ID not found error*/
}

SYSCALL_DEFINE3(peek_msg_421, unsigned long, id, unsigned char __user*, msg, long, n){
    mbox_t *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    mail_t *curr_mail;
    long len_to_compare;
    
    if (msg == NULL){
        return -EFAULT;
    }

    if(!access_ok(msg, n)){
        return -EFAULT;
    }

    down_read(&lock);
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, mbox_t, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            if (list_empty(&curr_mbox->mail_list)==1){
                up_read(&lock);
                return -ENOENT; /*Mailbox has no mail in it*/
            } else { 
                down_read(&curr_mbox->mail_lock);
                list_for_each_entry(curr_mail, &curr_mbox->mail_list, head_of_msgs){
                    if(copy_to_user(msg, curr_mail->message, n)){
                        up_read(&curr_mbox->mail_lock);
                        up_read(&lock);
                        return -EFAULT;
                    } else {
                        len_to_compare = curr_mail->len;
                        /*Unlock here cause you aren't deleting from the list*/
                        up_read(&curr_mbox->mail_lock);
                    
                        if(len_to_compare < n){
                            up_read(&lock);
                            return len_to_compare; /*successful return*/
                        }
                        up_read(&lock);
                        return n; /*succeful return*/
                    }
                }
            }
        }
    }
    up_read(&lock);
    return -ENOENT; /*ID not found error*/
}

SYSCALL_DEFINE1(count_msg_421, unsigned long, id){    
    mbox_t *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    long num_msgs;

    down_read(&lock);
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, mbox_t, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            num_msgs = curr_mbox->num_of_msgs;
            up_read(&lock);
            return num_msgs;
        }
    }
    up_read(&lock);
    return -ENOENT; /*ID not found error*/
}

SYSCALL_DEFINE1(len_msg_421, unsigned long, id){    
    mbox_t *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    mail_t *curr_mail; 
    long curr_msg_len;

    down_read(&lock);
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, mbox_t, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){
            if (list_empty(&curr_mbox->mail_list)==1){
                up_read(&lock);
                return -ENOENT; /*Mailbox is empty*/
            } else {
                down_read(&curr_mbox->mail_lock);
                list_for_each_entry(curr_mail, &curr_mbox->mail_list, head_of_msgs){
                    curr_msg_len = curr_mail->len;
                    up_read(&curr_mbox->mail_lock);
                    up_read(&lock);
                    /*Only need first message so you can return here*/
                    return curr_msg_len;
                }
            }
        }
    }
    up_read(&lock);
    return -ENOENT; /*ID not found error*/
}

SYSCALL_DEFINE1(print_mbox_421, unsigned long, id){
    mbox_t *curr_mbox;
    struct list_head *ptr;
    unsigned long curr_id;
    mail_t *curr_mail;
    long curr_msg_len;
    long num_of_msgs;
    long count = 1;
    int i;

    down_read(&lock);
    list_for_each(ptr, &head){
        curr_mbox = list_entry(ptr, mbox_t, list);
        curr_id = curr_mbox->ID;
        if (curr_id == id){ 
            down_read(&curr_mbox->mail_lock);
            num_of_msgs = curr_mbox->num_of_msgs;
            list_for_each_entry(curr_mail, &curr_mbox->mail_list, head_of_msgs){
                curr_msg_len = curr_mail->len;

                for(i = 0; i < curr_msg_len; i++){
                    /*Max 16 bytes per line*/
                    if(i != 0 && i%16==0){
                        printk("\n");
                    }
                    printk(KERN_CONT"%02x ", curr_mail->message[i]);
                }
                /*Seperate messages*/
                if(count == num_of_msgs){
                    /*skip, doesn't print --- after last message*/
                } else {
                    printk("\n---\n");
                    count++;
                }
            }
            up_read(&curr_mbox->mail_lock);
            up_read(&lock);

            printk("\n");
            return 0; /*Successful return*/
        }
    }
    up_read(&lock);
    return -ENOENT; /*ID not found error*/
}