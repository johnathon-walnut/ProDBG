extern crate minifb;

use minifb::{Window, Key, Scale, WindowOptions, Menu};
use minifb::{MENU_KEY_CTRL};

const WIDTH: usize = 640;
const HEIGHT: usize = 360;

const MENU_TEST_ID: usize = 1;
const OTHER_MENU_ID: usize = 2;
const COLOR_0_ID: usize = 3;
const COLOR_1_ID: usize = 4;
const COLOR_2_ID: usize = 5;
const CLOSE_MENU_ID: usize = 6;

fn main() {
    let mut buffer: Vec<u32> = vec![0; WIDTH * HEIGHT];

    let mut window = Window::new("Noise Test - Press ESC to exit",
                                 WIDTH,
                                 HEIGHT,
                                 WindowOptions {
                                     resize: true,
                                     scale: Scale::X2,
                                     ..WindowOptions::default()
                                 })
                         .expect("Unable to Open Window");

    let mut menu = Menu::new("Test").unwrap();
    let mut sub = Menu::new("Select Color").unwrap();

    sub.add_item("Color 0", COLOR_0_ID).shortcut(Key::F1, 0).build();
    sub.add_item("Color 1", COLOR_1_ID).shortcut(Key::F2, 0).build();
    sub.add_item("Color 2", COLOR_2_ID).shortcut(Key::F7, 0).build();

    menu.add_item("Menu Test", MENU_TEST_ID).shortcut(Key::W, MENU_KEY_CTRL).build();

    menu.add_separator();

    menu.add_item("Other Menu", OTHER_MENU_ID).shortcut(Key::W, MENU_KEY_CTRL).build();
    menu.add_item("Remove Menu", CLOSE_MENU_ID).shortcut(Key::R, 0).build();

    menu.add_sub_menu("Sub Test", &sub);

    let menu_handle = window.add_menu(&menu);

    let mut color_mul = 1;

    while window.is_open() && !window.is_key_down(Key::Escape) {
        for y in 0..HEIGHT {
            for x in 0..WIDTH {
                buffer[(y * WIDTH) + x] = (((x ^ y) & 0xff) * color_mul) as u32;
            }
        }

        window.is_menu_pressed().map(|menu_id| {
            match menu_id {
                COLOR_0_ID => {
                    color_mul = 0xfe0000;
                }
                COLOR_1_ID => {
                    color_mul = 0xff00;
                }
                COLOR_2_ID => {
                    color_mul = 1;
                }
                CLOSE_MENU_ID => {
                    println!("remove menu");
                    window.remove_menu(menu_handle);
                }
                _ => (),
            }

            println!("Menu id {} pressed", menu_id);
        });

        window.get_keys().map(|keys| {
            for t in keys {
                match t {
                    Key::W => println!("holding w!"),
                    Key::T => println!("holding t!"),
                    _ => (),
                }
            }
        });

        window.update_with_buffer(&buffer);
    }
}