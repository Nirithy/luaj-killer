local function multiply(a, b)
    local result = 0
    for i=1,b do
        result = result + a
    end
    return result
end
local t = { a = 1, b = 2, mult = multiply }
return t
