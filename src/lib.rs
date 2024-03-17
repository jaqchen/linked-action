// created by yejq.jiaqiang@gmail.com
// Licensed under the Apache License, Version 2.0 (the "License");

// borrowed from https://rust-lang.github.io/rust-bindgen/tutorial-4.html
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/rustex.rs"));

// use std::borrow::{Cow, ToOwned};

impl Clone for rext_var {
	fn clone(&self) -> Self {
		let mut rvar = rext_var::new();
		let rtype = self.mvar_type & MVALUE_TYPE_MASK;
		if rtype != MVALUE_TYPE_STRING {
			rvar.mvar_type = rtype;
			unsafe { rvar.mvar_val.mval_uint = self.mvar_val.mval_uint };
		} else {
			let mut plen: std::os::raw::c_uint = 0;
			let strp = unsafe { rext_var_str(self as *const rext_var, &mut plen as *mut std::os::raw::c_uint) };
			if strp == std::ptr::null_mut() {
				panic!("Error, failed to duplicate rext_var string!");
			}
			rvar.mvar_type = (plen << 0x4) | MVALUE_TYPE_STRING;
			rvar.mvar_val.mval_strp = strp;
		}
		rvar
	}
}

impl rext_var {
	pub fn new() -> Self {
		unsafe { std::mem::zeroed() }
	}

	pub fn as_string(&self) -> Option<String> {
		let rtype = self.mvar_type & MVALUE_TYPE_MASK;
		if rtype != MVALUE_TYPE_STRING {
			return None;
		}

		unsafe {
			let rvptr = self.mvar_val.mval_strp as *mut u8;
			let rvlen = (self.mvar_type >> 0x4) as usize;
			let mut rvbuf: Vec<u8> = Vec::with_capacity(rvlen + 1);
			for i in 0..rvlen {
				rvbuf.push(*rvptr.byte_add(i));
			}
			Some(String::from_utf8_lossy(&rvbuf).into_owned())
		}
	}

	pub fn as_bool(&self) -> Option<bool> {
		let rtype = self.mvar_type & MVALUE_TYPE_MASK;
		if rtype != MVALUE_TYPE_BOOL {
			None
		} else {
			let mbool = unsafe { self.mvar_val.mval_bool != 0 };
			Some(mbool)
		}
	}

	pub fn new_string(val: &[u8]) -> Self {
		let mut rvar: rext_var = unsafe { std::mem::zeroed() };
		let retval = unsafe { rext_var_strbuf(
			&mut rvar as *mut rext_var,
			val.as_ptr() as *const std::os::raw::c_char,
			val.len() as std::os::raw::c_uint) };
		if retval < 0 {
			panic!("Error, failed to duplicate string of length: {}", val.len());
		}
		rvar
	}

	pub fn new_bool(val: bool) -> Self {
		let mut rvar: rext_var = unsafe { std::mem::zeroed() };
		rvar.mvar_type = MVALUE_TYPE_BOOL;
		rvar.mvar_val.mval_bool = if val { 1 } else { 0 };
		rvar
	}

	pub fn new_int(val: i64) -> Self {
		let mut rvar: rext_var = unsafe { std::mem::zeroed() };
		rvar.mvar_type = MVALUE_TYPE_INT64;
		rvar.mvar_val.mval_int = val as std::os::raw::c_longlong;
		rvar
	}

	pub fn new_uint(val: u64) -> Self {
		let mut rvar: rext_var = unsafe { std::mem::zeroed() };
		rvar.mvar_type = MVALUE_TYPE_UINT64;
		rvar.mvar_val.mval_uint = val as std::os::raw::c_ulonglong;
		rvar
	}

	pub fn new_f32(val: f32) -> Self {
		let mut rvar: rext_var = unsafe { std::mem::zeroed() };
		rvar.mvar_type = MVALUE_TYPE_F32;
		rvar.mvar_val.mval_f32 = val;
		rvar
	}

	pub fn new_f64(val: f64) -> Self {
		let mut rvar: rext_var = unsafe { std::mem::zeroed() };
		rvar.mvar_type = MVALUE_TYPE_F64;
		rvar.mvar_val.mval_f64 = val;
		rvar
	}
}

impl Drop for rext_var {
	fn drop(&mut self) {
		unsafe { rext_var_free(self as *mut Self) };
	}
}
