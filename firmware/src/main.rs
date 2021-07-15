#![no_std]
#![no_main]

mod device_class;

extern crate panic_semihosting;

use core::str;
use cortex_m_rt::entry;
use cortex_m::asm::delay;
use cortex_m_semihosting::{ debug, hprintln };

use stm32f4xx_hal::{
    stm32,
    prelude::*,
    otg_fs::{ USB, UsbBus },
};

use embedded_hal::digital::v2::OutputPin;
use usb_device::prelude::*;

use device_class::{ ChipDevice, USB_CLASS_CUSTOM };


/*
use embedded_hal::blocking::delay::DelayMs;

struct CustomDelay(Delay);

impl DelayMs<u64> for CustomDelay {
    fn delay_ms(&mut self, ms: u64) {
        self.0.delay_ms(ms as u32)
    }
}
*/


fn busy_delay(delay: usize) {
    for _ in 0..delay { }
}


static mut EP_MEMORY: [u32; 1024] = [0; 1024];

#[entry]
fn main() -> ! {
    let dp = stm32::Peripherals::take().unwrap();
    let cp = stm32::CorePeripherals::take().unwrap();

    let mut rcc = dp.RCC.constrain();

    // Configure the clocks
    let clocks = rcc
        .cfgr
        .use_hse(8.mhz())
        .sysclk(48.mhz())
        .pclk1(24.mhz())
        .freeze();

    // Fetch the port devices we'll need
    let mut gpioa = dp.GPIOA.split();
    let mut gpiob = dp.GPIOB.split();
    let mut gpioc = dp.GPIOC.split();

    //let mut delay = CustomDelay(Delay::new(cp.SYST, clocks));



    let usb = USB {
        usb_global: dp.OTG_FS_GLOBAL,
        usb_device: dp.OTG_FS_DEVICE,
        usb_pwrclk: dp.OTG_FS_PWRCLK,
        pin_dm: gpioa.pa11.into_alternate_af10(),
        pin_dp: gpioa.pa12.into_alternate_af10(),
        hclk: clocks.hclk(),
    };

    let usb_bus = UsbBus::new(usb, unsafe { &mut EP_MEMORY });

    let mut device = ChipDevice::new(&usb_bus);

    let mut usb_dev = UsbDeviceBuilder::new(&usb_bus, UsbVidPid(0x1234, 0x1234))
        .manufacturer("Fake company")
        .product("Enumeration test")
        .serial_number("TEST")
        .device_class(USB_CLASS_CUSTOM)
        .build();


    loop {
        if usb_dev.poll(&mut [&mut device]) {
            let mut buf = [0u8; 64];

            match device.read_packet(&mut buf) {
                Ok(count) if count > 0 => {
                    hprintln!("recv: {}", str::from_utf8(&buf[0..count]).unwrap());
                    device.write_packet("Ok\n".as_bytes());
                },
                Ok(_) | Err(UsbError::WouldBlock) => { },
                Err(_) => {  }
            }
        }
    }

    /*
    //let gpioa = peripherals.GPIOA.split(); // + sets RCC->AHB1ENR GPIOA bit
    // .into_push_pull_output performs three steps
    // 1) set PUPDR: 00 -> no pull-up, no pull-down
    // 2) set OTYPER: 0 -> output push-pull
    // 3) set MODER: 01 -> general purpose output mode
    let mut led = gpioa.pa6.into_push_pull_output();

    let mut on = false;
    let mut i = 0;
    loop {
        i += 1;
        if i >= 1_000_000_000 {
            i = 0;

            if on {
                on = false;
                led.set_low().unwrap();
            } else {
                on = true;
                led.set_high().unwrap();
            }
        }
    }
    */
}
