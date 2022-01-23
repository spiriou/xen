# PCIe/GPU passthough for Xen

This project attempts to enable PCIe passthrough support on Xen for better GPU passthrough experience.

### How to build?

```
    ./configure \
        --prefix=/usr \
        --sbindir=/usr/bin \
        --libdir=/usr/lib \
        --with-rundir=/run \
        --enable-systemd \
        --disable-qemu-traditional \
        --disable-stubdom \
        --with-sysconfig-leaf-dir=conf.d \
        --enable-ovmf \
        --enable-githttp
```

### Example of VM configuration

```
name="xen-pcie"
builder="hvm"
memory=8192
vcpus=8
acpi=1
boot="dc"
serial='stdio'
xen_platform_pci=0

# This is required for Q35. Reserve room for ECAM at 0xB000000
mmio_hole=2048
device_model_machine="q35"
bios='ovmf'
# ovmf needs to use OvmfXenQ35.dsc config
bios_path_override='/path/to/OVMF.fd'

vif = ['bridge=xenbr0,bus=pci.1']

pci=['01:00.0,permissive=1,romfile=/path/to/gpu_vbios.bin', # GPU device
     '03:00.0,permissive=1',
     '00:14.0,permissive=1']
gfx_passthru=1
vga="none"

# Configure disk, bootable image if no NVMe is passthru
# disk=['phy:file:/path/to/image.iso,hdc:cdrom,r']
# disk=['tap:aio:/dev/sdb,hda']

device_model_args_hvm = [
  # Optional config for seabios
  # '-fw_cfg', 'name=bootsplash.jpg,file=bootsplash.jpg',

  # Debug OVMF
  '-chardev', 'file,id=debugcon,path=/var/log/xen/ovmf.log',
  '-device', 'isa-debugcon,iobase=0x402,chardev=debugcon',
  '-global', 'ICH9-LPC.disable_s3=1', '-global', 'ICH9-LPC.disable_s4=1',
  '-d', 'guest_errors',

  # Device configuration
  '-device', 'pcie-root-port,port=16,chassis=2,id=pci.1,bus=pcie.0,multifunction=on,addr=0x8',
  '-device', 'pcie-root-port,port=16,chassis=3,id=pci.2,bus=pcie.0,addr=0x8.0x1',
  '-device', 'qemu-xhci,p2=15,p3=15,id=usb,bus=pci.2',

  # USB devices shared with VM if USB root PCIe device is not passtru
  # '-usb',
  '-device', 'usb-host,hostbus=1,hostaddr=4',
  '-device', 'usb-host,hostbus=1,hostaddr=7',
]

```
