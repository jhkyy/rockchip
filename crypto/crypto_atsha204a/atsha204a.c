/*
 * atsha204a driver
 *
 * Copyright (C) 2016 SENSETIME, Inc.
 *
 */

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h> 
#include <asm/system_misc.h>
#include <linux/cdev.h>  
#include <linux/fs.h>  
#include <linux/delay.h>

#include "atsha204a.h"
#include "sha204_helper.h"
#include "atsha204a_ioctl_code.h"

#if 0
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif


static struct i2c_client *g_atsha204a_i2c = NULL;

/*
 * CRC计算，从atsha204代码库移植过来
 */
static void sha204c_calculate_crc(unsigned char length, unsigned char *data, unsigned char *crc) 
{
	unsigned char counter;
	unsigned short crc_register = 0;
	unsigned short polynom = 0x8005;
	unsigned char shift_register;
	unsigned char data_bit, crc_bit;
	for (counter = 0; counter < length; counter++) 
	{
		for (shift_register = 0x01; shift_register > 0x00; shift_register <<= 1) 
		{
			data_bit = (data[counter] & shift_register) ? 1 : 0;
			crc_bit = crc_register >> 15;
			// Shift CRC to the left by 1.
			crc_register <<= 1;
			if ((data_bit ^ crc_bit) != 0)
			{
				crc_register ^= polynom;
			}
		}
	}
	crc[0] = (unsigned char) (crc_register & 0x00FF);
	crc[1] = (unsigned char) (crc_register >> 8);
}

/*
 * CRC校验，从atsha204代码库移植过来
 */
static unsigned char sha204c_check_crc(unsigned char length, unsigned char  *data)
{
	unsigned char  crc[2];
	sha204c_calculate_crc(length-2, data, crc);
	return (crc[0] == data[length - 2] && crc[1] == data[length - 1]) ? SHA204_SUCCESS : SHA204_BAD_CRC;
}

static int atsha204a_reset(const struct i2c_client *i2c)
{
    struct i2c_adapter *adap=i2c->adapter;
    struct i2c_msg msg;
    int ret;
    char *tx_buf = (char *)kzalloc(1, GFP_KERNEL);

    if(!i2c)
        return -EIO;
    if(!tx_buf)
        return -ENOMEM;

    tx_buf[0] = 0x0;

    msg.addr = i2c->addr;
    msg.flags = i2c->flags;
    msg.len = 1;
    msg.buf = (char *)tx_buf;
    ret = i2c_transfer(adap, &msg, 1);

    msleep(3);
    kfree(tx_buf);
    return 1;
}

static int atsha204a_i2c_wakeup(const struct i2c_client *i2c)
{
    struct i2c_adapter *adap=i2c->adapter;
    struct i2c_msg msg;
    int ret;
    char *tx_buf = (char *)kzalloc(2, GFP_KERNEL);

    if(!i2c)
        return -EIO;
    if(!tx_buf)
        return -ENOMEM;

    tx_buf[0] = 0;
    tx_buf[1] = 0;

    msg.addr = 0;
    msg.flags = i2c->flags;
    msg.len = 2;
    msg.buf = (char *)tx_buf;
    ret = i2c_transfer(adap, &msg, 1);
    msleep(3);
    kfree(tx_buf);
    return 1;
}

/*
 * 通过I2C向atsha204写数据，考虑到唤醒原因，scl的速率为100k，不能设置太高
 */
static int atsha204a_i2c_write(const struct i2c_client *i2c, const char reg, int count, const char *buf)
{
    struct i2c_adapter *adap=i2c->adapter;
    struct i2c_msg msg;
    int ret;
    int i;

    char *tx_buf = (char *)kzalloc(count + 1, GFP_KERNEL);

    if(!i2c)
		return -EIO;
    if(!tx_buf)
        return -ENOMEM;

    tx_buf[0] = reg;
    memcpy(tx_buf+1, buf, count); 
 
    msg.addr = i2c->addr;
    msg.flags = i2c->flags;
    msg.len = count + 1;
    msg.buf = (char *)tx_buf;
    //msg.scl_rate = 100*1000;

    DBG(" \n");
    DBG("atsha204a_i2c_write write buff data : \n");
    for(i = 0; i < count + 1; i++){
    	DBG("0x%x  ", tx_buf[i]);
    }
    DBG(" \n");

    ret = i2c_transfer(adap, &msg, 1);
    DBG("write ret = %d \n", ret);
    kfree(tx_buf);
    return (ret == 1) ? count : ret;
 
}

/*
 * 通过I2C向atsha204读数据，考虑到唤醒原因，scl的速率为100k，不能设置太高
 */
static int atsha204a_i2c_read(struct i2c_client *i2c, char reg, int count, char *dest)
{
    int ret;
    int i;
//	int tryCount = 20;
    struct i2c_adapter *adap;
    struct i2c_msg msgs[2];

    if(!i2c)
		return -EIO;

    adap = i2c->adapter;		
    
    msgs[0].addr = i2c->addr;
    msgs[0].buf = &reg;
    msgs[0].flags = i2c->flags;
    msgs[0].len = 1;
    //msgs[0].scl_rate = 100*1000;
    
    msgs[1].buf = (char *)dest;
    msgs[1].addr = i2c->addr;
    msgs[1].flags = i2c->flags | I2C_M_RD;
    msgs[1].len = count;
    //msgs[1].scl_rate = 100*1000;
	

    ret = i2c_transfer(adap, msgs, 2);


    DBG(" \n");
    DBG("atsha204a_i2c_read read buff data : \n");
    for(i = 0; i < count; i++){
    	DBG("0x%x  ", msgs[1].buf[i]);
    }
    DBG(" \n");
    if(msgs[1].buf[0] == 0x04){
	    return -1;
	}else{
	    return ret; 
	}
	
}

/*
 * 发送atsha204操作命令，写命令的地址都为0x03
 */
static int atsha204a_send_command(struct i2c_client *i2c, struct atsha204a_command_packet command_packet)
{
	// 构建发送buffer
	unsigned char command_buffer_len = 7 + command_packet.data_len;
	unsigned char *command_buffer = (unsigned char *)kzalloc(command_buffer_len, GFP_KERNEL);

	command_buffer[0] = command_buffer_len;
	command_buffer[1] = command_packet.op_code;
	command_buffer[2] = command_packet.param1;
	command_buffer[3] = command_packet.param2[0];
	command_buffer[4] = command_packet.param2[1];

    if(command_packet.data_len > 0){
    	memcpy(command_buffer+5, command_packet.data, command_packet.data_len); 
    }

	sha204c_calculate_crc(command_buffer_len - 2, command_buffer, command_buffer + command_buffer_len - 2);

	// 通过IIC发送命令
	atsha204a_i2c_write(i2c,0x03,command_buffer_len,command_buffer);

	kfree(command_buffer);

    return 0;
}

/*
 * 读取操作命令的response内容，读取的地址都为0x00
 */
static int atsha204a_recv_command_response(struct i2c_client *i2c, unsigned char length, unsigned char *response_data)
{
	int ret = atsha204a_i2c_read(i2c, 0x0,length,response_data);
        if(ret <= 0){
            return SHA204_FAIL;
        }else{
	    return sha204c_check_crc(length,response_data) ? SHA204_SUCCESS : SHA204_BAD_RESPONSE;
        }
}

/*
 * 读取芯片内部序列号，序列号有9 Byte，前两个固定为0x01,0x23 最后一个固定为0xEE
 */
static int atsha204a_get_sn(struct i2c_client *i2c, unsigned char *sn_data)
{
	struct atsha204a_command_packet get_sn_command;
	char recev_buffer[35]  = {};

	get_sn_command.op_code   = SHA204_READ;    
	get_sn_command.param1    = SHA204_ZONE_COUNT_FLAG | SHA204_ZONE_CONFIG;  
	get_sn_command.param2[0] = 0;  
	get_sn_command.param2[1] = 0;  
	get_sn_command.data      = NULL;
	get_sn_command.data_len  = 0;
    
    atsha204a_send_command(i2c, get_sn_command);
    msleep(5);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(sn_data , recev_buffer + SHA204_BUFFER_POS_DATA, 4);
		memcpy(sn_data + 4 , recev_buffer + SHA204_BUFFER_POS_DATA + 8, 5);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;  
}

/*
 * 读取atsha204a的数据，对应分别有data\config\otp区，每次读取一个word( 4 Byte ) ,所以data长度为4
 */
static int atsha204a_run_read(struct i2c_client *i2c, unsigned char zone, unsigned char word_addr, unsigned char *data)
{
	struct atsha204a_command_packet read_data_command;
	char recev_buffer[7]  = {};

	read_data_command.op_code   = SHA204_READ;  
	read_data_command.param1    = zone;  
	read_data_command.param2[0] = word_addr;  
	read_data_command.param2[1] = 0;          
	read_data_command.data      = NULL;
	read_data_command.data_len  = 0;

    atsha204a_send_command(i2c, read_data_command);
    msleep(5);
    if(atsha204a_recv_command_response(i2c, 7, recev_buffer) == SHA204_SUCCESS){
    	memcpy(data , recev_buffer + SHA204_BUFFER_POS_DATA, 4);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;
}

/*
 * 读取配置区的数据
 */
static int __maybe_unused atsha204a_get_config_data(struct i2c_client *i2c, unsigned char word_addr, unsigned char *config_data)
{
	struct atsha204a_command_packet get_slot_command;
	char recev_buffer[7]  = {};

	get_slot_command.op_code   = SHA204_READ;  
	get_slot_command.param1    = SHA204_ZONE_CONFIG;  
	get_slot_command.param2[0] = word_addr;  
	get_slot_command.param2[1] = 0;          
	get_slot_command.data      = NULL;
	get_slot_command.data_len  = 0;

    atsha204a_send_command(i2c, get_slot_command);
    msleep(5);
    if(atsha204a_recv_command_response(i2c, 7, recev_buffer) == SHA204_SUCCESS){
    	memcpy(config_data , recev_buffer + SHA204_BUFFER_POS_DATA, 4);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;
}

/*
 * 读取slot的数据
 */
static int __maybe_unused atsha204a_get_slot_data(struct i2c_client *i2c, unsigned char slot_id, unsigned char *slot_data)
{
	struct atsha204a_command_packet get_slot_command;
	char recev_buffer[35]  = {};

	get_slot_command.op_code   = SHA204_READ;  
	get_slot_command.param1    = SHA204_ZONE_COUNT_FLAG | SHA204_ZONE_DATA; 
	get_slot_command.param2[0] = slot_id << 2;  
	get_slot_command.param2[1] = 0;  
	get_slot_command.data      = NULL;
	get_slot_command.data_len  = 0;

    atsha204a_send_command(i2c, get_slot_command);
    msleep(5);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(slot_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;
}

/*
 * 读取atsha204内部生成的随机数
 */
static int __maybe_unused atsha204a_run_random(struct i2c_client *i2c, unsigned char *random_data)
{
	struct atsha204a_command_packet get_random_command;
	char recev_buffer[35]  = {};

	get_random_command.op_code   = SHA204_RANDOM;    
	get_random_command.param1    = 0;  
	get_random_command.param2[0] = 0;  
	get_random_command.param2[1] = 0;  
	get_random_command.data      = NULL;
	get_random_command.data_len  = 0;

    atsha204a_send_command(i2c, get_random_command);
    msleep(60);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(random_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;
}

/*
 * 执行 atsha204 NONCE命令,NONCE会返回32位的随机数
 */
static int atsha204a_run_nonce(struct i2c_client *i2c, unsigned char mode, unsigned char *num_in, unsigned char *nonce_data)
{
	struct atsha204a_command_packet run_nonce_command;
	char recev_buffer[35]  = {};

	run_nonce_command.op_code   = SHA204_NONCE;    
	run_nonce_command.param1    = mode;  
	run_nonce_command.param2[0] = 0;  
	run_nonce_command.param2[1] = 0;  
	run_nonce_command.data      = num_in;
	run_nonce_command.data_len  = 20;

    atsha204a_send_command(i2c, run_nonce_command);
    msleep(80);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(nonce_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;
}

/*
 * 执行 atsha204 MAC命令，MAC命令返回sha256算法生成的摘要
 */
static int atsha204a_run_mac(struct i2c_client *i2c, unsigned char mode, unsigned char slot_id, unsigned char challenge_len, unsigned char *challenge, unsigned char *mac_data)
{
	struct atsha204a_command_packet run_mac_command;
	char recev_buffer[35]  = {};

	run_mac_command.op_code   = SHA204_MAC;  
	run_mac_command.param1    = mode;  
	run_mac_command.param2[0] = 0;  
	run_mac_command.param2[1] = slot_id;  
	run_mac_command.data      = challenge;
	run_mac_command.data_len  = challenge_len;

    atsha204a_send_command(i2c, run_mac_command);
    msleep(50);
    if(atsha204a_recv_command_response(i2c, 35, recev_buffer) == SHA204_SUCCESS){
    	memcpy(mac_data , recev_buffer + SHA204_BUFFER_POS_DATA, 32);
		return SHA204_SUCCESS;
    }
    return SHA204_FAIL;
}


/*
 * 模拟加密验证的流程
 */
static int __maybe_unused atsha204a_mac(struct i2c_client *i2c, unsigned char slot_id, unsigned char *secret_key, unsigned char num_in_len, unsigned char *num_in, unsigned char challenge_len, unsigned char *challenge)
{
	//int i;
	char nonce_response[32]  = {};
	char mac_response[32]    = {};
	uint8_t soft_digest [32];						//!< Software calculated digest
	struct sha204h_nonce_in_out nonce_param;		//!< Parameter for nonce helper function
	struct sha204h_mac_in_out mac_param;			//!< Parameter for mac helper function
	struct sha204h_temp_key tempkey;				//!< tempkey parameter for nonce and mac helper function

	// atsha204a run nonce
	if (atsha204a_run_nonce(i2c,NONCE_MODE_NO_SEED_UPDATE,num_in,nonce_response) == SHA204_FAIL)	{
		DBG("atsha204a_run_nonce fail \n ");
		return SHA204_FAIL;
	}

    // simulate run nonce
    nonce_param.mode = NONCE_MODE_NO_SEED_UPDATE;
	nonce_param.num_in = num_in;	
	nonce_param.rand_out = nonce_response;	
	nonce_param.temp_key = &tempkey;
	if (sha204h_nonce(&nonce_param) == SHA204_FAIL){
		DBG("sha204h_nonce fail \n ");
		return SHA204_FAIL;
	}

	// atsha204 run mac
	if (atsha204a_run_mac(i2c, MAC_MODE_BLOCK2_TEMPKEY, slot_id, challenge_len, challenge,mac_response) == SHA204_FAIL)	{
		DBG("atsha204a_run_mac fail \n ");
		return SHA204_FAIL;
	}
	
	// simulate run mac
    mac_param.mode = MAC_MODE_BLOCK2_TEMPKEY;
	mac_param.key_id = slot_id;
	mac_param.challenge = challenge;
	mac_param.key = secret_key;
	mac_param.otp = NULL;
	mac_param.sn = NULL;
	mac_param.response = soft_digest;
	mac_param.temp_key = &tempkey;
	if (sha204h_mac(&mac_param) == SHA204_FAIL){
		DBG("sha204h_mac fail \n ");
		return SHA204_FAIL;
	}
    
	// compare result
    if(memcmp(soft_digest,mac_response,32) == 0){
    	DBG(" mac succ ! \n");
    }else{
		DBG(" mac fail ! \n");
	}

    return SHA204_SUCCESS;
}

/*
 * Probe中读取加密芯片的序列号进行判断芯片是否存在
 */
static int atsha204a_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	unsigned char sn_data[9] = {0x00};
	
	g_atsha204a_i2c = i2c;
    
    // 读取加密芯片序列号进行判断
    if(atsha204a_get_sn(g_atsha204a_i2c, sn_data) == SHA204_SUCCESS){
		if(sn_data[0] == 0x01 && sn_data[1] == 0x23 && sn_data[8] == 0xEE){
			printk(" atsha204a_probe succ ! \n");
			return 0;
		}
	}
	
	printk(" atsha204a_probe fail -- no atsha204a found ! \n");
	return -1;
}
 
static int  atsha204a_remove(struct i2c_client *i2c){
	return 0;
}

static const struct of_device_id atsha204a_of_match[] = {
	{ .compatible = "atmel,atsha204a"},
};

static const struct i2c_device_id atsha204a_id[] = {
       { "atsha204a", 0 },
       { }
};  
   
MODULE_DEVICE_TABLE(i2c, atsha204a_id);

static struct i2c_driver atsha204a_driver = {
	.driver = {
		.name = "atsha204a",
		.owner = THIS_MODULE, 
		.of_match_table =of_match_ptr(atsha204a_of_match),
	},
	.probe    = atsha204a_probe,
	.remove   = atsha204a_remove,
	.id_table = atsha204a_id,
};
  

/***************************** driver device file for atsha204a control ******************************/

static int atsha204a_open(struct inode* inode, struct file* filp) {      
	DBG("atsha204a_open. \n");
    return 0;  
}  
   
static int atsha204a_release(struct inode* inode, struct file* filp) {  
	DBG("atsha204a_release. \n");
    return 0;  
}  

static ssize_t atsha204a_read(struct file *filp, char __user *buf, size_t sz, loff_t *off)  {  
	DBG("atsha204a_read. \n");
    return sizeof(int);  
}  
  
static ssize_t atsha204a_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)  {    
	DBG("atsha204a_write. \n");
    return sizeof(int);  
}  

static long atsha204a_ioctl(struct file* filp, unsigned int cmd, unsigned long data)  {    
    int i = 0;
	int ret = 0;
	int retryCount = 3;
	DBG("atsha204a_ioctl. %d\n" , cmd);
 
	switch (cmd) {
	case ATSHA204A_SN_CMD:
	    for(i = 0; i < retryCount; i++){
			atsha204a_i2c_wakeup(g_atsha204a_i2c);
			atsha204a_reset(g_atsha204a_i2c);
			ret = atsha204a_get_sn(g_atsha204a_i2c, ((struct atsha204a_sn*)data)->out_response);
			if(ret > 0) return 1;
                        msleep(50);
		}
		break;
	case ATSHA204A_NONCE_CMD:
	    for(i = 0; i < retryCount; i++){
			atsha204a_i2c_wakeup(g_atsha204a_i2c);
			atsha204a_reset(g_atsha204a_i2c);
			ret = atsha204a_run_nonce(g_atsha204a_i2c, ((struct atsha204a_nonce*)data)->in_mode, ((struct atsha204a_nonce*)data)->in_numin, ((struct atsha204a_nonce*)data)->out_response);
			if(ret > 0) return 1;
                        msleep(50);
		}
		break;
	case ATSHA204A_MAC_CMD:
	    for(i = 0; i < retryCount; i++){
			atsha204a_i2c_wakeup(g_atsha204a_i2c);
			atsha204a_reset(g_atsha204a_i2c);
			ret = atsha204a_run_mac(g_atsha204a_i2c, ((struct atsha204a_mac*)data)->in_mode,((struct atsha204a_mac*)data)->in_slot,((struct atsha204a_mac*)data)->in_challenge_len,((struct atsha204a_mac*)data)->in_challenge,((struct atsha204a_mac*)data)->out_response);
			if(ret > 0) return 1;
			msleep(50);
		}
		break;

	case ATSHA204A_WRITE_CMD:
		return 1;
	case ATSHA204A_READ_CMD:
	    for(i = 0; i < retryCount; i++){
			atsha204a_i2c_wakeup(g_atsha204a_i2c);
			atsha204a_reset(g_atsha204a_i2c);
			ret = atsha204a_run_read(g_atsha204a_i2c, ((struct atsha204a_read*)data)->in_zone, ((struct atsha204a_read*)data)->in_word_addr, ((struct atsha204a_read*)data)->out_response);
			if(ret > 0) return 1;
			msleep(50);
		}
		break;
	default:
		return 0;
	}
    return 0;  
} 


static int    atsha204a_major = 0;
static int    atsha204a_minor = 0;
static dev_t  ndev; 
static struct device *atsha204a_device = NULL;
static struct class *crypto_class      = NULL;  
static struct cdev *atsha204a_dev      = NULL;  
static struct file_operations atsha204a_driver_fops = {
    .owner   = THIS_MODULE,
    .open    = atsha204a_open,  
    .release = atsha204a_release, 
    .read    = atsha204a_read,  
    .write   = atsha204a_write, 
    .unlocked_ioctl   = atsha204a_ioctl, 
};

/*
 * 驱动模块加载初始化
 */
static int __init atsha204a_module_init(void)
{
	int ret = -1; 
	dev_t devno;

	ret = i2c_add_driver(&atsha204a_driver);
	if (ret != 0){
		pr_err("failed to register I2C driver: %d\n", ret);
		goto clear_return;
	}

    // 设备号相关 
    ret = alloc_chrdev_region(&ndev, 0, 1, "atsha204a");
    if(ret < 0){
    	pr_err("failed to alloc chrdev : %d\n", ret);
    	goto destroy_i2c;
    } 
    atsha204a_major = MAJOR(ndev);  
    atsha204a_minor = MINOR(ndev);  

	// 分配atsha204a设备结构体变量
    atsha204a_dev = cdev_alloc();
    if(!atsha204a_dev) { 
    	ret = -ENOMEM; 
        pr_err(KERN_ALERT"failed to alloc mem for atsha204a_dev \n");  
        goto unregister_chrdev;
    }  

    // 初始化注册设备
    devno = MKDEV(atsha204a_major, atsha204a_minor);  
    cdev_init(atsha204a_dev, &atsha204a_driver_fops);  
    atsha204a_dev->ops = &atsha204a_driver_fops;

    ret = cdev_add(atsha204a_dev,devno, 1);  
    if (ret){
        pr_err(KERN_ALERT"failed to add char dev for atsha204a \n");  
        goto unregister_chrdev;
    } 

	// 注册一个类，使mdev可以在"/sys/class"目录下 面建立设备节点 
    crypto_class = class_create(THIS_MODULE, "crypto");
    if (IS_ERR(crypto_class)){
        ret = -1;
        DBG("failed to create atsha204a moudle class.\n");
        goto destroy_cdev;
    }

    // 创建一个设备节点，节点名为atsha204a ,如果成功，它将会在/dev/atsha204a设备。
    atsha204a_device = device_create(crypto_class, NULL, MKDEV(atsha204a_major, 0), "atsha204a", "atsha204a");
    if (IS_ERR(atsha204a_device)){
    	ret = -1;
        DBG("failed to create atsha204a device .\n");
        goto destroy_class;
    }

    return 0;
/*
destroy_device:  
    device_destroy(crypto_class, ndev);  
*/  
destroy_class:  
    class_destroy(crypto_class);  
  
destroy_cdev:  
    cdev_del(atsha204a_dev); 

unregister_chrdev:  
    unregister_chrdev_region(ndev, 1);

destroy_i2c:  
    i2c_del_driver(&atsha204a_driver); 

clear_return:
    return ret;
} 

static void __exit atsha204a_module_exit(void)
{
	device_destroy(crypto_class, ndev);  
	class_destroy(crypto_class);  
	cdev_del(atsha204a_dev); 
	unregister_chrdev_region(ndev, 1);
	i2c_del_driver(&atsha204a_driver); 
}

late_initcall(atsha204a_module_init);
module_exit(atsha204a_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liujinjin <liujinjin@sensetime.com>");
MODULE_DESCRIPTION("atsha204a crypto driver");

