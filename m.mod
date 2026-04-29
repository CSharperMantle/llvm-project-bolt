!mod$ v1 sum:444ba0b36254725f
module m
integer(4)::int_array(1_8:10_8)
real(4)::real_array(1_8:5_8,1_8:5_8)
complex(4)::complex_array(1_8:3_8)
logical(4)::logical_array(1_8:4_8)
character(10_4,1)::char_array(1_8:2_8)
integer(4),target::target_array(1_8:8_8)
integer(4),allocatable::alloc_array(:)
integer(4)::scalar_var
integer(4),bind(c,name="c_int_array")::bind_c_int_array(1_8:10_8)
real(4),bind(c,name="c_real_array")::bind_c_real_array(1_8:5_8)
integer(4),bind(c,name="c_init_array")::bind_c_init_array(1_8:5_8)
real(4),bind(c,name="c_real_init")::bind_c_real_init(1_8:3_8)
integer(4),bind(c,name="c_scalar")::bind_c_scalar
end
