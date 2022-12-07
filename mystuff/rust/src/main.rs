#[derive(Debug)]
pub struct Foo {
    len: usize
}

#[derive(Debug)]
pub struct Foo2 {
    len: usize
}

pub trait IsFoo {
    fn getlen(&self) -> usize;
}

macro_rules! impl_with_field {
    ($($t:ty),+ $(,)?) => ($(
        impl IsFoo for $t {
            fn getlen(&self) -> usize {
                self.len
            }
        }
    )+)
}
impl_with_field!(Foo, Foo2);

fn main() {
    // let mut f: Foo;
    let x: i64;
    let mut y = Foo {len: 0 };
    println!("Hello, world! {:?}", y);
    y.len = 1;
    println!("Hello, world! {:?}", y.getlen());

}
