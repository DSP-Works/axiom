use crate::codegen::util;
use inkwell::builder::Builder;
use inkwell::context::Context;
use inkwell::module::Module;
use inkwell::types::{BasicType, StructType};
use inkwell::values::{BasicValue, PointerValue, StructValue};

#[derive(Debug, Clone)]
pub struct TupleValue {
    pub val: PointerValue,
}

impl TupleValue {
    pub fn get_type(context: &Context, inner_types: &[&BasicType]) -> StructType {
        context.struct_type(inner_types, false)
    }

    pub fn new(val: PointerValue) -> Self {
        TupleValue { val }
    }

    pub fn new_from(
        module: &Module,
        alloca_builder: &mut Builder,
        builder: &mut Builder,
        values: &[PointerValue],
    ) -> TupleValue {
        let enum_types: Vec<_> = values
            .iter()
            .map(|val| val.get_type().element_type())
            .collect();
        let inner_types: Vec<_> = enum_types.iter().map(|val| val as &BasicType).collect();
        let new_type = TupleValue::get_type(&module.get_context(), &inner_types);
        let new_tuple = TupleValue::new(alloca_builder.build_alloca(&new_type, "tuple"));

        for (index, val) in values.iter().enumerate() {
            let target_ptr = new_tuple.get_item_ptr(builder, index);
            util::copy_ptr(builder, module, *val, target_ptr);
        }

        new_tuple
    }

    pub fn get_const(context: &Context, values: &[&BasicValue]) -> StructValue {
        context.const_struct(values, false)
    }

    pub fn get_item_ptr(&self, builder: &mut Builder, index: usize) -> PointerValue {
        unsafe { builder.build_struct_gep(&self.val, index as u32, "tuple.item.ptr") }
    }
}
