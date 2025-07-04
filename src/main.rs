use std::io;
use std::io::Write;

fn main() {
    loop {
        print!("Input your age: ");
        io::stdout().flush().unwrap();
        let mut age = String::new();
        io::stdin().read_line(&mut age).expect("No");
        let age: i8 = match age.trim().parse() {
                Ok(num) => num,
                Err(_) => {
                    println!("You absolute failure");
                    break;
                }
        };
        if age < 18 {
            println!("child");
            break;
        }
        println!("adult");
        break; 
    }
}