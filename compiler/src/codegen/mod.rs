pub mod block;
mod builder_context;
pub mod controls;
pub mod converters;
pub mod data_analyzer;
pub mod editor;
pub mod functions;
pub mod globals;
pub mod intrinsics;
pub mod math;
mod module_iterator;
mod object_cache;
mod optimizer;
pub mod root;
pub mod runtime_lib;
pub mod surface;
mod target_properties;
pub mod util;
pub mod values;

pub use self::builder_context::{build_context_function, BuilderContext};
pub use self::module_iterator::{ModuleFunctionIterator, ModuleGlobalIterator};
pub use self::object_cache::ObjectCache;
pub use self::optimizer::Optimizer;
pub use self::target_properties::TargetProperties;

use std::fmt;

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum LifecycleFunc {
    Construct,
    Update,
    Destruct,
}

impl fmt::Display for LifecycleFunc {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match self {
            LifecycleFunc::Construct => write!(f, "construct"),
            LifecycleFunc::Update => write!(f, "update"),
            LifecycleFunc::Destruct => write!(f, "destruct"),
        }
    }
}
