use crate::codegen::util;
use inkwell::builder::Builder;
use inkwell::context::Context;
use inkwell::module::Module;
use inkwell::types::StructType;
use inkwell::values::{IntValue, PointerValue, StructValue, VectorValue};
use std::borrow::Borrow;

#[derive(Debug, Clone)]
pub struct NumValue {
    pub val: PointerValue,
}

impl NumValue {
    pub fn get_type(context: &Context) -> StructType {
        context.struct_type(
            &[&context.f64_type().vec_type(2), &context.i8_type()],
            false,
        )
    }

    pub fn new(val: PointerValue) -> Self {
        NumValue { val }
    }

    pub fn new_undef(context: &Context, alloca_builder: &mut Builder) -> Self {
        let num_type = NumValue::get_type(context);
        NumValue::new(alloca_builder.build_alloca(&num_type, "num"))
    }

    pub fn new_copy(
        module: &Module,
        alloca_builder: &mut Builder,
        builder: &mut Builder,
        base: &NumValue,
    ) -> Self {
        let result = NumValue::new_undef(module.get_context().borrow(), alloca_builder);
        base.copy_to(builder, module, &result);
        result
    }

    pub fn get_const(context: &Context, left: f64, right: f64, form: u8) -> StructValue {
        NumValue::get_type(context).const_named_struct(&[
            &util::get_const_vec(context, left, right),
            &context.i8_type().const_int(u64::from(form), false),
        ])
    }

    pub fn copy_to(&self, builder: &mut Builder, module: &Module, other: &NumValue) {
        util::copy_ptr(builder, module, self.val, other.val)
    }

    pub fn load(&self, builder: &mut Builder) -> StructValue {
        builder
            .build_load(&self.val, "num.loaded")
            .into_struct_value()
    }

    pub fn store(&mut self, builder: &mut Builder, value: StructValue) {
        builder.build_store(&self.val, &value);
    }

    pub fn get_vec_ptr(&self, builder: &mut Builder) -> PointerValue {
        unsafe { builder.build_struct_gep(&self.val, 0, "num.vec.ptr") }
    }

    pub fn get_vec(&self, builder: &mut Builder) -> VectorValue {
        let vec = self.get_vec_ptr(builder);
        builder.build_load(&vec, "num.vec").into_vector_value()
    }

    pub fn set_vec(&self, builder: &mut Builder, value: VectorValue) {
        let vec = self.get_vec_ptr(builder);
        builder.build_store(&vec, &value);
    }

    pub fn get_form_ptr(&self, builder: &mut Builder) -> PointerValue {
        unsafe { builder.build_struct_gep(&self.val, 1, "num.form.ptr") }
    }

    pub fn get_form(&self, builder: &mut Builder) -> IntValue {
        let vec = self.get_form_ptr(builder);
        builder.build_load(&vec, "num.form").into_int_value()
    }

    pub fn set_form(&self, builder: &mut Builder, value: IntValue) {
        let vec = self.get_form_ptr(builder);
        builder.build_store(&vec, &value);
    }
}
