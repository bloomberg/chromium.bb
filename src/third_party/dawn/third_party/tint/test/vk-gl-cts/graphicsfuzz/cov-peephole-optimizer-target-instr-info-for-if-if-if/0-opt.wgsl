type Arr = [[stride(16)]] array<i32, 3>;

[[block]]
struct buf0 {
  x_GLF_uniform_int_values : Arr;
};

type Arr_1 = [[stride(16)]] array<f32, 2>;

[[block]]
struct buf1 {
  x_GLF_uniform_float_values : Arr_1;
};

var<private> x_GLF_color : vec4<f32>;

[[group(0), binding(0)]] var<uniform> x_6 : buf0;

var<private> gl_FragCoord : vec4<f32>;

[[group(0), binding(1)]] var<uniform> x_9 : buf1;

fn main_1() {
  var i : i32;
  let x_37 : i32 = x_6.x_GLF_uniform_int_values[1];
  let x_38 : f32 = f32(x_37);
  x_GLF_color = vec4<f32>(x_38, x_38, x_38, x_38);
  let x_41 : i32 = x_6.x_GLF_uniform_int_values[1];
  i = x_41;
  loop {
    let x_46 : i32 = i;
    let x_48 : i32 = x_6.x_GLF_uniform_int_values[2];
    if ((x_46 < x_48)) {
    } else {
      break;
    }
    let x_52 : f32 = gl_FragCoord.y;
    let x_54 : f32 = x_9.x_GLF_uniform_float_values[0];
    if ((x_52 < x_54)) {
      let x_59 : f32 = gl_FragCoord.x;
      let x_61 : f32 = x_9.x_GLF_uniform_float_values[0];
      if ((x_59 < x_61)) {
        return;
      }
      let x_66 : f32 = x_9.x_GLF_uniform_float_values[1];
      let x_68 : f32 = x_9.x_GLF_uniform_float_values[1];
      if ((x_66 > x_68)) {
        return;
      }
      discard;
    }
    let x_73 : f32 = x_9.x_GLF_uniform_float_values[1];
    let x_75 : f32 = x_9.x_GLF_uniform_float_values[0];
    if ((x_73 > x_75)) {
      let x_80 : i32 = x_6.x_GLF_uniform_int_values[0];
      let x_83 : i32 = x_6.x_GLF_uniform_int_values[1];
      let x_86 : i32 = x_6.x_GLF_uniform_int_values[1];
      let x_89 : i32 = x_6.x_GLF_uniform_int_values[0];
      x_GLF_color = vec4<f32>(f32(x_80), f32(x_83), f32(x_86), f32(x_89));
      break;
    }
    let x_93 : f32 = x_9.x_GLF_uniform_float_values[0];
    if ((x_93 < 0.0)) {
      discard;
    }

    continuing {
      let x_97 : i32 = i;
      i = (x_97 + 1);
    }
  }
  return;
}

struct main_out {
  [[location(0)]]
  x_GLF_color_1 : vec4<f32>;
};

[[stage(fragment)]]
fn main([[builtin(position)]] gl_FragCoord_param : vec4<f32>) -> main_out {
  gl_FragCoord = gl_FragCoord_param;
  main_1();
  return main_out(x_GLF_color);
}
