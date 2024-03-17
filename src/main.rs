// Created by yejq.jiaqiang@gmail.com
// Licensed under the Apache License, Version 2.0 (the "License");

use std::io::Write;
use std::os::raw::c_int;
use std::collections::HashMap;

use serde::Deserialize;
use serde_json::Value;

use linked_action::*;

static RULEFUNC_PFX: &str = "rulefunc_";
static RULEFUNC_ALL: &str = "rulefunc_all";
static TEMPVAL_NAM: &str = "link_tval_";

#[derive(Deserialize)]
struct LinkedAction {
	#[serde(rename = "rule")]
	rule_bool:       String,

	#[serde(rename = "true")]
	action_true:    String,

	#[serde(default, rename = "false")]
	action_false:   Option<String>,
}

struct LinkedVar {
	upval:          usize, // upvalue index in Lua script
	value:          f64,
}

struct LinkedInfo {
	vars:           HashMap<String, LinkedVar>,
	script:         String, // constructed Lua value
	actions:        Vec<LinkedAction>,
	luast:          rext_any_t,
}

impl LinkedVar {
	fn new(val: f64) -> Self {
		Self { upval: 0, value: val }
	}
}

impl LinkedInfo {
	fn new(cfgpath: &str) -> Option<Self> {
		let mtinfo = match std::fs::metadata(cfgpath) {
			Ok(metainfo) => metainfo,
			Err(err) => {
				eprintln!("Error, failed check file '{}': {}", cfgpath, err);
				return None;
			},
		};

		if !mtinfo.is_file() || mtinfo.len() >= 0x100000 {
			eprintln!("Error, invalid filetype or size '{}': {}", cfgpath, mtinfo.is_file());
			return None;
		}

		let cfgdat: String = match std::fs::read_to_string(cfgpath) {
			Ok(dat) => dat,
			Err(err) => {
				eprintln!("Error, failed to read '{}': {}", cfgpath, err);
				return None;
			},
		};

		let mut cfgs: Value = match serde_json::from_str(&cfgdat) {
			Ok(configs) => configs,
			Err(err) => {
				eprintln!("Error, failed to decode as JSON '{}': {}", cfgpath, err);
				return None;
			},
		};

		if !cfgs.is_object() {
			eprintln!("Error, invalid data from '{}'", cfgpath);
			return None;
		}

		let vars = match cfgs.get("linked-vars") {
			Some(vars) if vars.is_object() => vars,
			_ => {
				eprintln!("Error, 'linked-vars' not found in '{}'", cfgpath);
				return None;
			},
		};

		let linkvars: HashMap<String, LinkedVar> = vars.as_object().unwrap().iter()
			.filter_map(|(lv_key, lv_val)| -> Option<(String, LinkedVar)> {
				if lv_val.is_number() { Some((lv_key.clone(),
					LinkedVar::new(lv_val.as_f64().unwrap()))) } else { None }
			})
			.collect();

		if linkvars.is_empty() {
			eprintln!("Error, no variable found in '{}'", cfgpath);
			return None;
		}

		let linkacts: Value = match cfgs.get_mut("linked-action") {
			Some(acts) if acts.is_array() => acts.take(),
			_ => {
				eprintln!("Error, no valid linked-actions defined.");
				return None;
			},
		};

		let actions: Vec<LinkedAction> = match serde_json::from_value::<Vec<LinkedAction>>(linkacts) {
			Ok(acts) if !acts.is_empty() => acts,
			Ok(_) => {
				eprintln!("Error, no linked-action found.");
				return None;
			},
			Err(err) => {
				eprintln!("Error, failed to deserialize linked-actions: {}", err);
				return None;
			},
		};

		Some(Self { vars: linkvars, script: String::new(),
			actions, luast: std::ptr::null_mut() })
	}

	fn build_script(&mut self) {
		let mut est_size: usize = self.vars.keys().map(|key| key.len()).sum();
		// 16 -> length of 'local X = YYYYYY'
		est_size += 16 * self.vars.len();

		est_size += self.actions.iter().map(|la| la.rule_bool.len()).sum::<usize>();
		// 40 -> length of 'function rulefunc_X()\n    return YY\nend\n'
		est_size += 40 * self.actions.len();

		let lua_buf: Vec<u8> = Vec::with_capacity(est_size);
		let mut wrt_buf = std::io::BufWriter::with_capacity(2048, lua_buf);

		let _ = wrt_buf.write("\n".as_bytes());
		// write all the upvalues
		for (key, val) in &self.vars {
			let _ = write!(wrt_buf, "local {} = {}\n", key, val.value);
		}

		// write all linked-action rule-functions
		let mut count: usize = 1;
		for la in &self.actions {
			let _ = write!(wrt_buf, "function {}{}()\n\treturn {}\nend\n",
				RULEFUNC_PFX, count, la.rule_bool);
			count += 1;
		}

		// write a function containing all the upvalues
		let _ = write!(wrt_buf, "function {}()\n\tlocal {} = 0\n", RULEFUNC_ALL, TEMPVAL_NAM);
		for key in self.vars.keys() {
			let _ = write!(wrt_buf, "\t{} = {} + {}\n", TEMPVAL_NAM, TEMPVAL_NAM, key);
		}
		let _ = write!(wrt_buf, "\treturn {}\nend\n", TEMPVAL_NAM);
		wrt_buf.flush().unwrap();
		let lua_buf: Vec<u8> = wrt_buf.into_inner().unwrap();

		self.script = String::from_utf8_lossy(&lua_buf).into_owned();
	}

	fn fetch_indices(&mut self) -> u32 {
		let mut count: u32 = 0;
		let mut idx: c_int = 1;
		let rulefun = rext_var::new_string(RULEFUNC_ALL.as_bytes());
		loop {
			let mut valn = rext_var::new();
			let error = unsafe { rext_upval_get(self.luast,
				&rulefun as *const rext_var, idx, &mut valn as *mut rext_var) };
			if error < 0 {
				break;
			}

			if let Some(varn) = valn.as_string() {
				if let Some(lv) = self.vars.get_mut(&varn) {
					println!("Inserting {} => {}", varn, idx);
					lv.upval = idx as usize;
					count += 1;
				}
			}
			idx += 1;
		}
		count
	}

	fn act(&mut self, vmap: &HashMap<&str, f64>) -> bool {
		let rulefun = rext_var::new_string(RULEFUNC_ALL.as_bytes());
		// update upvalues in the Lua state machine
		for (key, val) in vmap {
			let lvar = self.vars.get_mut(*key);
			if lvar.is_none() {
				eprintln!("Warning, variable not found: {}, ignored", key);
				continue;
			}

			let lvar = lvar.unwrap();
			lvar.value = *val;
			let fval = rext_var::new_f64(*val);
			let error = unsafe { rext_upval_set(self.luast,
				&rulefun as *const rext_var, lvar.upval as i32,
				&fval as *const rext_var) };
			if error < 0 {
				eprintln!("Error, failed to update Lua value: {}", key);
			}
		}

		let mut fcount = 0;
		for la in &self.actions {
			fcount = fcount + 1;
			let mut resp = rext_var::new();
			let funn = format!("{}{}", RULEFUNC_PFX, fcount);
			let fun = rext_var::new_string(funn.as_bytes());
			let rval = unsafe { rext_call(self.luast,
				&fun as *const rext_var, std::ptr::null(),
				&mut resp as *mut rext_var) };
			if let Some(really) = resp.as_bool() {
				println!("{:5}: {} => {}", really, rval, la.rule_bool);
			} else {
				println!("function '{}' has returned: {}", funn, rval);
			}
		}

		println!();
		true
	}
}

fn main() {
	let mut linked = match LinkedInfo::new("./linked-action-config.json") {
		Some(info) => info,
		None => std::process::exit(1),
	};

	linked.build_script();
	println!("linked info script: {}", linked.script);

	let mut error :c_int = 0;
	let script = rext_var::new_string(linked.script.as_bytes());
	let luast: rext_any_t = unsafe { rext_lua_new(&script as *const rext_var, &mut error) };
	if error != 0 {
		eprintln!("Error, failed to create lua-state machine: {}", error);
		std::process::exit(2);
	}

	println!("Created Lua-state machine: {:p}", luast);
	linked.luast = luast;
	println!("Lua upvalues found: {}", linked.fetch_indices());

	let t_map: HashMap<&str, f64> = HashMap::from([
		("ex_var1", 1f64),
		("ex_var2", 2f64),
		("ex_var3", 3f64),
		("ex_var4", 4f64),
		("ex_var5", 5f64),
	]);
	linked.act(&t_map);

	let t_map: HashMap<&str, f64> = HashMap::from([
		("ex_var1", 2f64),
		("ex_var2", 3f64),
		("ex_var3", 4f64),
		("ex_var4", 5f64),
		("ex_var5", 6f64),
	]);
	linked.act(&t_map);
}
