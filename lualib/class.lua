function class(classname, super)
    local cls = {super = super}
    if super then
        for k, v in pairs(super) do
            cls[k] = v
        end
    end 
    cls.__cname = classname
    if not cls.ctor then
        -- add default constructor
        cls.ctor = function() end
    end
    cls.new = function(...)
        local instance = {}
        for k, v in pairs(cls) do
            instance[k] = v
        end
        instance.class = cls
        instance:ctor(...)
        return instance
    end

    return cls
end
