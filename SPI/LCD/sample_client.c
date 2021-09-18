#include <linux/err.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include "spi_char.h"

#define LCD_DC_PIN		26
#define LCD_RESET_PIN		46
#define LCD_CS_PIN		65

#define PIN_NUMBER 3
static unsigned int gpioButton[PIN_NUMBER] = {LCD_DC_PIN, LCD_RESET_PIN, LCD_CS_PIN};

unsigned char txbuf[5] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E};
unsigned char rxbuf[5];
unsigned char rxbuf2[5];

typedef enum pin_state {
	Low,
	High
} pin_state_t;

struct sample_data {
	struct spi_device *spi;
	struct spi_message msg;
	struct spi_transfer transfer[2];
	u8 tx_buf;
	u8 rx_buf[2];
	// character device driver files
	dev_t devt;
	struct cdev cdev;
	struct class *class;
};


static ssize_t sample_read(struct file* f, char *buf, size_t count, loff_t *f_pos)
{
	int res;
	struct sample_data *dev = (struct sample_data*) (f->private_data);
	
	struct spi_transfer tr = 
    	{
		.tx_buf	= &txbuf,
		.rx_buf = &rxbuf,
		.len = 1,
	};
	res = spi_sync_transfer(dev->spi, &tr, 1);
	printk(KERN_INFO "spi_sync_transfer Write Result %d value: %u %u %u %u %u\n", res, txbuf[0], txbuf[1], txbuf[2], txbuf[3], txbuf[4]);
	printk(KERN_INFO "spi_sync_transfer Got Result %d value: %u %u %u %u %u\n", res, rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3], rxbuf[4]);
	rxbuf[0] = 0;
	
	mdelay(2);
	
	res =  spi_write(dev->spi, &txbuf, sizeof(txbuf[0]));
	printk(KERN_INFO "spi_write Write Result %d value: %u %u %u %u %u\n", res, txbuf[0], txbuf[1], txbuf[2], txbuf[3], txbuf[4]);
	/* spi_read to read the data form our spi */
	mdelay(2);
	res = spi_read(dev->spi, &rxbuf2, sizeof(rxbuf2[0]));
	printk(KERN_INFO "Got Result %d value: %u %u %u %u %u\n", res, rxbuf2[0], rxbuf2[1], rxbuf2[2], rxbuf2[3], rxbuf2[4]);
	rxbuf2[0] = 0;
	
	return 0;
}

static int sample_close(struct inode *i, struct file *file)
{
	return 0;
}

static int sample_open(struct inode *i, struct file *f)
{
	struct sample_data *data = container_of(i->i_cdev, struct sample_data, cdev);
	if(data == NULL)
	{
		printk("data is null\n");
		return -1;
	}
	f->private_data = data;

	return 0;
}

struct file_operations fops = {
	.open = sample_open,
	.release = sample_close,
	.read = sample_read,
};

void spiWrite_command(void *dev, unsigned char c)
{
	int res;
	struct spi_transfer tr = 
    	{
		.tx_buf	= &c,
		.rx_buf = &rxbuf,
		.len = 1,
	};
	
	gpio_set_value(LCD_DC_PIN, Low);
	//spi_write(dev, &c, sizeof(c));
	res = spi_sync_transfer(dev, &tr, 1);
	//printk(KERN_INFO "spi_sync_transfer Got Result %d value: %u %u %u %u %u\n", res, rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3], rxbuf[4]);
}

void spiWrite_data(void *dev, unsigned char c)
{
	int res;
	struct spi_transfer tr = 
    	{
		.tx_buf	= &c,
		.rx_buf = &rxbuf,
		.len = 1,
	};
	
	gpio_set_value(LCD_DC_PIN, High);
	//spi_write(dev, &c, sizeof(c));
	res = spi_sync_transfer(dev, &tr, 1);
	//printk(KERN_INFO "spi_sync_transfer Got Result %d value: %u %u %u %u %u\n", res, rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3], rxbuf[4]);
}

void Address_set(void *dev, unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2)
{
        spiWrite_command(dev, 0x2a);
	spiWrite_data(dev, x1>>8);
	spiWrite_data(dev, x1);
	spiWrite_data(dev, x2>>8);
	spiWrite_data(dev, x2);
        spiWrite_command(dev, 0x2b);
	spiWrite_data(dev, y1>>8);
	spiWrite_data(dev, y1);
	spiWrite_data(dev, y2>>8);
	spiWrite_data(dev, y2);
	spiWrite_command(dev, 0x2c); 							 
}

void H_line(void *dev, unsigned int x, unsigned int y, unsigned int l, unsigned int c)                   
{	
	unsigned int i,j;
	
	gpio_set_value(LCD_CS_PIN, Low);  //CS
	
	spiWrite_command(dev, 0x02c); //write_memory_start
	//digitalWrite(RS,HIGH);
	l=l+x;
	Address_set(dev,x,y,l,y);
	j=l*2;
	for(i=1;i<=j;i++)
	{
		spiWrite_data(dev, c);
	}
	
	gpio_set_value(LCD_CS_PIN, High);  //CS 
}

void V_line(void *dev, unsigned int x, unsigned int y, unsigned int l, unsigned int c)                   
{	
	unsigned int i,j;
	
	gpio_set_value(LCD_CS_PIN, Low);  //CS
	
	spiWrite_command(dev, 0x02c); //write_memory_start
	//digitalWrite(RS,HIGH);
	l=l+y;
	Address_set(dev,x,y,x,l);
	j=l*2;
	for(i=1;i<=j;i++)
	{ 
		spiWrite_data(dev, c);
	}
	
	gpio_set_value(LCD_CS_PIN, High);  //CS 
}

void Rect(void *dev, unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int c)
{
	H_line(dev, x  , y  , w, c);
	H_line(dev, x  , y+h, w, c);
	V_line(dev, x  , y  , h, c);
	V_line(dev, x+w, y  , h, c);
}

static int sample_probe(struct spi_device *spi)
{
	struct sample_data *data;
	int init_result;
	int res, ret;
	int i = 0;
	
	ENTER();

	data = devm_kzalloc(&spi->dev, sizeof(struct sample_data), GFP_KERNEL);
	data->spi = spi;
	spi_set_drvdata(spi,data);

	init_result = alloc_chrdev_region(&data->devt, 0, 1, "spi_smp");

	if (0 > init_result)
	{
		printk(KERN_ALERT "Device Registration failed\n");
		unregister_chrdev_region(data->devt, 1);
		return -1;
	}
	printk("Major Nr: %d\n", MAJOR(data->devt));
	
	cdev_init(&data->cdev, &fops);

	// Register the file ops
	if (cdev_add(&data->cdev, data->devt, 1) == -1)
	{
		printk( KERN_ALERT "Device addition failed\n" );
		device_destroy(data->class, data->devt);
		class_destroy(data->class);
		unregister_chrdev_region(data->devt, 1 );
		return -1;
	}

	if ((data->class = class_create(THIS_MODULE, "spisample")) == NULL)
	{
		printk( KERN_ALERT "Class creation failed\n" );
		unregister_chrdev_region(data->devt, 1);
		return -1;
	}
	 //Create the device file with name spi_smp0
	if (device_create(data->class, NULL, data->devt, NULL, "spi_smp%d", 0) == NULL)
	{
		printk( KERN_ALERT "Device creation failed\n" );
		class_destroy(data->class);
		unregister_chrdev_region(data->devt, 1);
		return -1;
	}
	
	// Check invalid gpio
	for (i = 0; i< PIN_NUMBER; i++)
	{
		if (!gpio_is_valid(gpioButton[i]))
		{
			pr_info("This gpio pin %d is not valid\n", gpioButton[i]);
			return -1;
		}
	}
	
	// Request
	for (i = 0; i< PIN_NUMBER; i++)
	{
		ret = gpio_request(gpioButton[i], "lcd");
		if (ret < 0)
		{
			pr_info("Failed to request GPIO\n");
			return -1;
		}
		//config pin is output
		gpio_direction_output(gpioButton[i], 0);
	}
	
	//LCD reset
	gpio_set_value(LCD_RESET_PIN, High);
	mdelay(5);
	gpio_set_value(LCD_RESET_PIN, Low);
	mdelay(15);
	gpio_set_value(LCD_RESET_PIN, High);
	mdelay(15);
	
	gpio_set_value(LCD_CS_PIN, Low);  //CS
	
	spiWrite_command(spi, 0xCB);  
	spiWrite_data(spi, 0x39); 
	spiWrite_data(spi, 0x2C); 
	spiWrite_data(spi, 0x00); 
	spiWrite_data(spi, 0x34); 
	spiWrite_data(spi, 0x02); 

	spiWrite_command(spi, 0xCF);  
	spiWrite_data(spi, 0x00); 
	spiWrite_data(spi, 0XC1); 
	spiWrite_data(spi, 0X30); 

	spiWrite_command(spi, 0xE8);  
	spiWrite_data(spi, 0x85); 
	spiWrite_data(spi, 0x00); 
	spiWrite_data(spi, 0x78); 

	spiWrite_command(spi, 0xEA);  
	spiWrite_data(spi, 0x00); 
	spiWrite_data(spi, 0x00); 

	spiWrite_command(spi, 0xED);  
	spiWrite_data(spi, 0x64); 
	spiWrite_data(spi, 0x03); 
	spiWrite_data(spi, 0X12); 
	spiWrite_data(spi, 0X81); 

	spiWrite_command(spi, 0xF7);  
	spiWrite_data(spi, 0x20); 

	spiWrite_command(spi, 0xC0);    //Power control 
	spiWrite_data(spi, 0x23);   //VRH[5:0] 

	spiWrite_command(spi, 0xC1);    //Power control 
	spiWrite_data(spi, 0x10);   //SAP[2:0];BT[3:0] 

	spiWrite_command(spi, 0xC5);    //VCM control 
	spiWrite_data(spi, 0x3e);   //Contrast
	spiWrite_data(spi, 0x28); 

	spiWrite_command(spi, 0xC7);    //VCM control2 
	spiWrite_data(spi, 0x86);   //--

	spiWrite_command(spi, 0x36);    // Memory Access Control 
	spiWrite_data(spi, 0x48);   

	spiWrite_command(spi, 0x3A);    
	spiWrite_data(spi, 0x55); 

	spiWrite_command(spi, 0xB1);    
	spiWrite_data(spi, 0x00);  
	spiWrite_data(spi, 0x18); 

	spiWrite_command(spi, 0xB6);    // Display Function Control 
	spiWrite_data(spi, 0x08); 
	spiWrite_data(spi, 0x82);
	spiWrite_data(spi, 0x27);  

	spiWrite_command(spi, 0x11);    //Exit Sleep 
	mdelay(120); 

	spiWrite_command(spi, 0x29);    //Display on 
	spiWrite_command(spi, 0x2c);
	
	gpio_set_value(LCD_CS_PIN, High);  //CS
	
	Rect(spi, 50, 100, 150, 200,50000); // rectangle at x, y, with, hight, color
	
	//struct spi_transfer tr = 
    	//{
	//	.tx_buf	= &txbuf,
	//	.rx_buf = &rxbuf,
	//	.len = 1,
	//};
	//spi_sync_transfer(spi, &tr, 1);
	//res = spi_write_then_read(spi, &buf, sizeof(buf), &recv, sizeof(recv));
	//res =  spi_write(spi, &buf, sizeof(buf));
	//printk(KERN_INFO "Write Result %d value: %u %u %u %u %u\n", res, txbuf[0], txbuf[1], txbuf[2], txbuf[3], txbuf[4]);
	/* spi_read to read the data form our spi */
	//mdelay(2);
	//res = spi_read(spi, &recv, sizeof(recv));
	//printk(KERN_INFO "Got Result %d value: %u %u %u %u %u\n", res, rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3], rxbuf[4]);
	//rxbuf[0] = 0;
	
	return 0;
}

static int sample_remove(struct spi_device *spi)
{
	struct sample_data *data = spi_get_drvdata(spi);

	// Delete the device file & the class
	device_destroy(data->class, data->devt);
   	
	//  Unregister file operations
	class_destroy(data->class);
	cdev_del(&data->cdev);
   	
	// Unregister character driver
	unregister_chrdev_region(data->devt, 1 );

	return 0;
}

//Populate the id table as per dtb
static const struct spi_device_id sample_id[] = {
	{ "my_spi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, sample_id);

// Populate the spi_driver data structure
static struct spi_driver sample_driver = {
	.driver = {
		.name = "sample_client",
		.owner = THIS_MODULE,
	},
	.probe = sample_probe,
	.remove = sample_remove,
	.id_table = sample_id,
};
module_spi_driver(sample_driver);

MODULE_AUTHOR("TechoGenius Academy");
MODULE_DESCRIPTION("Sample Client driver");
MODULE_LICENSE("GPL v2");
