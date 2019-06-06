function invoke(a, b, c)
  -- call a C++ function
	avg, sum = avg_Cpp(a, b, c)
	return avg, sum
end