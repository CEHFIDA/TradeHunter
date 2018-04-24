use std::io::{self, BufRead};
use std::path::Path;
use std::fs::File;

#[allow(dead_code)]
fn lines_from_file<P>(filename: P) -> Result<io::Lines<io::BufReader<File>>, io::Error>
where
    P: AsRef<Path>,
{
    let file = try!(File::open(filename));
    Ok(io::BufReader::new(file).lines())
}

fn main() {
    println!("Hello, world!");
}
