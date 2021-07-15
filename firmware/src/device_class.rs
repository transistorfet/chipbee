
use cortex_m_semihosting::hprintln;
use usb_device::class_prelude::*;

const MAX_PACKET_SIZE: u16 = 64;
pub const USB_CLASS_CUSTOM: u8 = 0xff;

pub struct ChipDevice<'a, B: UsbBus> {
    data_if: InterfaceNumber,
    read_ep: EndpointOut<'a, B>,
    write_ep: EndpointIn<'a, B>,
    ctrl_ep: EndpointOut<'a, B>,
}

impl <B: UsbBus> ChipDevice<'_, B> {
    pub fn new(alloc: &UsbBusAllocator<B>) -> ChipDevice<'_, B> {
        ChipDevice {
            data_if: alloc.interface(),
            read_ep: alloc.bulk(MAX_PACKET_SIZE),
            write_ep: alloc.bulk(MAX_PACKET_SIZE),
            ctrl_ep: alloc.control(MAX_PACKET_SIZE),
        }
    }

    pub fn read_packet(&mut self, data: &mut [u8]) -> Result<usize, UsbError> {
        self.read_ep.read(data)
    }

    pub fn write_packet(&mut self, data: &[u8]) -> Result<usize, UsbError> {
        self.write_ep.write(data)
    }
}

impl<B: UsbBus> UsbClass<B> for ChipDevice<'_, B> {
    fn get_configuration_descriptors(&self, writer: &mut DescriptorWriter) -> Result<(), UsbError> {
        writer.interface(self.data_if, USB_CLASS_CUSTOM, 0x00, 0x00)?;
        writer.endpoint(&self.write_ep)?;
        writer.endpoint(&self.read_ep)?;
        writer.endpoint(&self.ctrl_ep)?;
        Ok(())
    }

    fn reset(&mut self) {

    }

    fn control_in(&mut self, xfer: ControlIn<B>) {
        let req = xfer.request();

        if !(req.request_type == control::RequestType::Class
            && req.recipient == control::Recipient::Interface)
        {
            return;
        }

        match req.request {

            _ => { xfer.reject().ok(); },
        }
    }
}

