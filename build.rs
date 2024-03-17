// Created by yejq.jiaqiang@gmail.com
// Licensed under the Apache License, Version 2.0 (the "License");

fn main() {
	let cwd: String = match std::env::current_dir() {
		Ok(dir) => dir.to_string_lossy().into_owned(),
		Err(err) => {
			eprintln!("Error, cannot get current working directory: {}", err);
			std::process::exit(1);
		},
	};

	let rustex: &str = "./rustex/rustex.rs";
	let bindgen: bool = match std::fs::metadata(rustex) {
		Ok(md) if md.is_file() => false,
		_ => {
			let _ = std::fs::remove_file(rustex);
			let _ = std::fs::remove_dir_all(rustex);
			true
		},
	};

	if bindgen {
		let okay = std::process::Command::new("bindgen")
			.args(&["--no-copy", "rext_var", "--no-copy", "rext_val", "./rustex/rustex.h", "-o"])
			.arg(rustex)
			.spawn()
			.unwrap()
			.wait()
			.unwrap()
			.success();
		if !okay {
			std::process::exit(2);
		}
	}

	// copy rustex.rs to OUT_DIR
	if let Some(mut outdir) = std::env::var_os("OUT_DIR") {
		outdir.push("/rustex.rs");
		let res = std::fs::copy(rustex, &outdir);
		if res.is_err() {
			let error = res.unwrap_err();
			eprintln!("Error, failed to copy '{}' => '{:?}': {}", rustex, outdir, error);
			std::process::exit(3);
		}
	}

	// invoke GNU make utility to rebuild rustex
	let okay = std::process::Command::new("make")
		.args(&["-j1", "-C", "rustex", "clean", "all"])
		.spawn()
		.unwrap()
		.wait()
		.unwrap()
		.success();
	if !okay {
		std::process::exit(4);
	}

	println!("cargo:rustc-link-arg-bins=-lrustex");
	println!("cargo:rustc-link-arg-bins=-L{}/rustex", cwd);
	println!("cargo:rustc-link-arg-bins=-Wl,-rpath={}/rustex", cwd);
	println!("cargo:rerun-if-changed=./rustex/rustex.h");
	println!("cargo:rerun-if-changed=./rustex/rustex.c");
}
