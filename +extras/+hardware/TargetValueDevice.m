classdef TargetValueDevice < matlab.mixin.SetGet & matlab.mixin.Heterogeneous & extras.widgets.mixin.HasDeviceName
% Generic Class for devices which have a "Taret" set point & actual "Value"
%
%% Copyright 2019 Daniel T. Kovari, Emory University
%   All rights reserved.     
    properties (Abstract=true,SetAccess=protected,SetObservable=true,AbortSet=true)
        Value;
    end
    
    properties (SetAccess=protected,SetObservable=true,AbortSet=true)
        UpdatedAfterTargetChange;
    end
    
    properties (Abstract=true,SetObservable=true) %allow setting TargetValue to same TargetValue, that way wr message gets sent again
        Target;
    end
    
    properties (SetAccess=protected, SetObservable=true, AbortSet = true)
        Units = '';
        Limits = [-Inf,Inf];
        ValueSize = [1,1];
        ValueLabels = '';
    end
    
    methods
        function set.Limits(this,val)
            
            %validate
            ok = false;
            if isnumeric(val)&&numel(val)==2
                val = reshape(val,1,2);
                ok = true;
            elseif iscell(val) && all(size(val)==this.ValueSize)
                ok = true;
                for n=1:numel(val)
                    if ~isnumeric(val{n})||numel(val{n})~=2
                        ok=false;
                        break;
                    end
                    val{n} = reshape(val{n},1,2);
                end
            end
            assert(ok, 'Limits must be 1x2 numeric or cell array containing 1x2 numerics that is the same size as ValueSize');
            
            this.Limits = val;
        end
    end
end