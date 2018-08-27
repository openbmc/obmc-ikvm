#!/bin/sh

new_directory="/sys/kernel/config/usb_gadget/obmc_hid"

if [ -e "${new_directory}" ]; then
	exit 0
fi

# create gadget
original_directory="$(pwd)"
mkdir "${new_directory}"
cd "${new_directory}"

# add basic information
echo 0x0100 > bcdDevice
echo 0x0200 > bcdUSB
echo 0x00 > bDeviceClass
echo 0x00 > bDeviceProtocol
echo 0x00 > bDeviceSubClass
echo 0x0b > bMaxPacketSize0
echo 0x0104 > idProduct		# Multifunction Composite Gadget
echo 0x1d6b > idVendor		# Linux Foundation

# create English locale
mkdir strings/0x409

echo "OpenBMC" > strings/0x409/manufacturer
echo "virtual_input" > strings/0x409/product
echo "OBMC0001" > strings/0x409/serialnumber

# Create HID function
mkdir functions/hid.usb0

echo 1 > functions/hid.usb0/protocol
echo 11 > functions/hid.usb0/report_length
echo 1 > functions/hid.usb0/subclass

# Binary HID descriptor
echo -n -e '\x05\x01\x09\x06\xa1\x01\x85\x01\x05\x07\x19\xe0\x29\xe7\x15\x00\x25\x01\x95\x08\x75\x01\x81\x02\x95\x01\x75\x08\x81\x01\x95\x05\x75\x08\x15\x01\x25\x7f\x05\x07\x19\x01\x29\x7f\x81\x00\xc0\x05\x01\x09\x02\xa1\x01\x85\x02\x09\x01\xa1\x00\x05\x09\x19\x01\x29\x03\x15\x00\x25\x01\x95\x03\x75\x01\x81\x02\x95\x01\x75\x05\x81\x03\x05\x01\x09\x30\x09\x31\x35\x00\x46\xff\x7f\x15\x00\x26\xff\x7f\x75\x10\x95\x02\x81\x02\xc0\xc0' > functions/hid.usb0/report_desc
# Create configuration
mkdir configs/c.1
mkdir configs/c.1/strings/0x409

echo 0x80 > configs/c.1/bmAttributes
echo 200 > configs/c.1/MaxPower
echo "" > configs/c.1/strings/0x409/configuration

# Link HID function to configuration
ln -s functions/hid.usb0 configs/c.1

# Enable gadget
dev_name="1e6a0000.usb-vhub"
i=0
num_ports=5
base_usb_dir="/sys/bus/platform/devices/${dev_name}/${dev_name}:p"
while [ $i -lt $num_ports ]; do
	port=$(($i + 1))
	i=$port
	if [ ! -e "${base_usb_dir}${port}/gadget/suspended" ]; then
		break
	fi
done
echo "${dev_name}:p${port}" > UDC

cd "${original_directory}"
