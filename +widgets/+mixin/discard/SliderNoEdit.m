classdef SliderNoEdit < uiw.abstract.JavaControl & uiw.mixin.HasValueEvents & uiw.mixin.HasCallback
    % SliderNoEdit - A numeric slider without editable text value
    %
    % Create a widget with a slider and edit text.
    %
    % Syntax:
    %           w = uiw.widget.Slider('Property','Value',...)
    %
    
    % Copyright 2017-2018 The MathWorks, Inc.
    %
    % Auth/Revision:
    %   MathWorks Consulting
    %   $Author: rjackey $
    %   $Revision: 253 $
    %   $Date: 2018-10-05 08:50:12 -0400 (Fri, 05 Oct 2018) $
    % 
    % Modified by D. Kovari, Emory University 2019
    % ---------------------------------------------------------------------
    
    %% Properties
    properties (AbortSet)
        Min (1,1) double {mustBeFinite} = 0 % Minimum value allowed
        Max (1,1) double {mustBeFinite} = 100 % Maximum value allowed
        MinTickStep (1,1) double {mustBeNonnegative, mustBeFinite} = 0 % Minimum tick step size
        SnapToTicks (1,1) logical = false % Slider snaps to tick marks [true|(false)]
        EnforceRange (1,1) logical = true % Require edit field to also be in range?
        Orientation char {mustBeMember(Orientation,{'horizontal','vertical'})} = 'horizontal' % Slider orientation [horizontal|vertical]
        ShowTicks (1,1) logical = true % Whether to show ticks [(true)|false]
        ShowLabels (1,1) logical = true % Whether to show tick labels [(true)|false]
        Focusable (1,1) logical = true % Allow slider to have focus border, and keyboard control [(true)|false]
        Value
    end
    properties
        
    end

    properties (AbortSet, Hidden)
        NotifyOnMotion (1,1) logical = false %Undocumented and may change
        LabelMode char {mustBeMember(LabelMode,{'auto','manual'})} = 'auto' %Undocumented and may change
        CustomLabels containers.Map = containers.Map %Undocumented and may change
    end
    
    properties (Access=protected)
        Multiplier (1,1) double {mustBePositive, mustBeFinite} = 1 % Multiplier used for internal calculation
        PendingValue
    end
    
    
    
    %% Constructor / Destructor
    methods
        
        function obj = SliderNoEdit(varargin)
            % Construct the control
            
            % Create the slider
            obj.createJControl('javax.swing.JSlider');
            obj.JControl.StateChangedCallback = @(h,e)onSliderMotion(obj,e);
            obj.JControl.MouseReleasedCallback = @(h,e)onSliderChanged(obj,e);
            obj.HGJContainer.Units = 'pixels';
            obj.JControl.setOpaque(false);
            
            % Use the default value
            obj.Value = obj.JControl.getValue();
            
            % Set properties from P-V pairs
            obj.assignPVPairs(varargin{:});
            
            % No new value is pending
            obj.PendingValue = obj.Value;
            
            % Assign the construction flag
            obj.IsConstructed = true;
            
            % Redraw the widget
            obj.onResized();
            obj.onEnableChanged();
            obj.onStyleChanged();
            obj.redraw();
            
        end % constructor
        
    end %methods - constructor/destructor
    
    
    
    %% Protected methods
    methods (Access=protected)
        
        function onValueChanged(obj,~)
            % Handle updates to value changes
            
            % No new value is pending
            obj.PendingValue = obj.Value;
            
            obj.redraw();
            
        end %function
        
        
        function redraw(obj)
            % Handle state changes that may need UI redraw
            
            % Ensure the construction is complete
            if obj.IsConstructed
                
                % Update slider value
                jValue = obj.Value * obj.Multiplier
                javaMethodEDT('setValue',obj.JControl,jValue);
                
                % Are we enforcing the range? If not, we need to recheck
                % coloring.
                if ~obj.EnforceRange
                    obj.onStyleChanged();
                end
                
            end %if obj.IsConstructed
            
        end %function
        
        
        function onResized(obj,~,~)
            % Handle changes to widget size
            
            % Ensure the construction is complete
            if obj.IsConstructed
                
                % Get widget dimensions
                [w,h] = obj.getInnerPixelSize;
                pad = obj.Padding;
                spc = obj.Spacing;
                
                % Calculate new positions
                if strcmpi(obj.Orientation,'horizontal')
                    javaMethodEDT('setOrientation',obj.JControl,false);
                    wT = 0;
                    pad = floor( min(pad, wT/8) );

                    div = w-wT-pad;
                    jPos = [1 1 (div-pad) h];
                else %vertical
                    javaMethodEDT('setOrientation',obj.JControl,true);
                    hT = 0;
                    pad = floor( min(spc/2, hT/8) );
                    if obj.FlipText
                        div = h-hT-pad;
                        jPos = [1 1 w (div-pad)];
                    else
                        div = hT+pad;
                        jPos = [1 1+(div+pad) w (h-div-pad)];
                    end
                end
                
                % Set positions
                obj.HGJContainer.Position = jPos;
                
                % Redraw ticks
                obj.redrawTicks();
                
                % Update slider value
                jValue = obj.Value * obj.Multiplier;
                javaMethodEDT('setValue',obj.JControl,jValue);
                
            end %if obj.IsConstructed
            
        end %function
        
        
        function onEnableChanged(obj,~)
            % Handle updates to Enable state
            
            % Ensure the construction is complete
            if obj.IsConstructed
                
                % Call superclass methods
                onEnableChanged@uiw.abstract.JavaControl(obj);
                
            end %if obj.IsConstructed
            
        end %function
        
        
        function onStyleChanged(obj,~)
            % Handle updates to style and value validity changes
            
            % Ensure the construction is complete
            if obj.IsConstructed
                
                % Override edit text colors
%                 obj.TextForegroundColor = obj.ForegroundColor;
%                 obj.TextBackgroundColor = obj.BackgroundColor;
                
                % Call superclass methods
                onStyleChanged@uiw.abstract.JavaControl(obj);
                
                
            end %if obj.IsConstructed
            
        end %function
        
        
        function StatusOk = checkValue(~, value)
            % Return true if the value is valid
            
            StatusOk = isnumeric(value) && isscalar(value) && ~isnan(value);
            
        end %function
        
        
        
        function onSliderMotion(obj,~)
            if obj.IsConstructed 
                obj.PendingValue = obj.JControl.getValue() / obj.Multiplier;
                if obj.PendingValue ~= obj.Value
                    if obj.NotifyOnMotion %Undocumented - may be removed
                        obj.onSliderChanged();
                    else
                        obj.onValueChanging(obj.PendingValue);
                    end
                end
            end
        end
        
        
        function onSliderChanged(obj,~)
            % Handle interaction with slider
            newValue = obj.JControl.getValue() / obj.Multiplier;
            if ~isequal(newValue,obj.Value)
                evt = struct('Source', obj, ...
                    'Interaction', 'Slider Changed', ...
                    'OldValue', obj.Value, ...
                    'NewValue', newValue);
                obj.Value = newValue;
                obj.redraw();
                obj.callCallback(evt);
            end
        end %function
        
        function redrawTicks(obj)
            
            % We want to have up to 10 major ticks and five minor ticks in
            % between. We try to get major ticks on powers of ten.
            
            % Ensure the construction is complete
            if obj.IsConstructed
                
                % Get the widget width and use it to determine the maximum
                % number of tick marks. We allow a minimum of 25 pixels between
                % each major tick-mark.
                % Get widget dimensions
                [w,h] = obj.getInnerPixelSize;
                if strcmpi(obj.Orientation,'horizontal')
                    maxMajorTicks = floor(w/25);
                else
                    maxMajorTicks = floor(h/25);
                end
                maxMajorTicks = max(maxMajorTicks-1, 2);
                
                % Work out our desired spacing
                range = (obj.Max - obj.Min);
                major = power( 10, ceil( log10( range/100 ) ) );
                
                if major <= obj.MinTickStep
                    major = obj.MinTickStep;
                end
                
                % Increase the spacing until we have sufficiently few
                while range/major > maxMajorTicks
                    if range/(major*2) <= maxMajorTicks
                        major = major*2;
                    elseif range/(major*5) <= maxMajorTicks
                        major = major*5;
                    else
                        major = major*10;
                    end
                end
                
                % Minor ticks are 5 per major tick, or use minumum
                minor = max(obj.MinTickStep, major/5);
                
                % We need to use integers so use a multiplier if spacing is
                % fractional
                obj.Multiplier = max(1/minor, 1);
                mMin = obj.Min;
                mMax = obj.Max;
                jMin = mMin * obj.Multiplier;
                jMax = mMax * obj.Multiplier;
                
                % The Java integer equivalent of the tick spacing
                jMinor = minor * obj.Multiplier;
                jMajor = major * obj.Multiplier;
                
                % Now set the min/max and spacing
                javaMethodEDT('setMinimum',obj.JControl,jMin);
                javaMethodEDT('setMaximum',obj.JControl,jMax);
                javaMethodEDT('setMinorTickSpacing',obj.JControl,jMinor);
                javaMethodEDT('setMajorTickSpacing',obj.JControl,jMajor);
                
                % Set ticks display on/off
                javaMethodEDT('setPaintTicks',obj.JControl,obj.ShowTicks);
                javaMethodEDT('setPaintLabels',obj.JControl,obj.ShowLabels);
                javaMethodEDT('setSnapToTicks',obj.JControl,obj.SnapToTicks);
                
                % The labels need to recreated to lie on the major ticks
                if obj.ShowTicks || obj.ShowLabels
                    
                    jHash = java.util.Hashtable();
                    fgCol = obj.ForegroundColor;
                    
                    if strcmpi(obj.LabelMode,'auto') || isempty(obj.CustomLabels.values)
                         
                        % Make the ticks fall on even multiples of major tick
                        % spacing. This only works if tick marks are off,
                        % as they do not have a way to offset them.
                        if mod(mMin,major)>0 && ~obj.ShowTicks
                            jFirstMajor = ceil(mMin/major)* major * obj.Multiplier;
                            jTicks = int32( jFirstMajor : jMajor : jMax );
                            if (jFirstMajor - jMin) < jMajor/2
                                jTicks(1) = jMin;
                            else
                                jTicks = [jMin jTicks];
                            end
                        else
                            jTicks = int32( jMin : jMajor : jMax );
                        end
                        
                        mTicks = double(jTicks) / obj.Multiplier;
                        
                        for idx=1:numel(jTicks)
                            jThisLabel = javax.swing.JLabel(num2str(mTicks(idx)));
                            jThisLabel.setForeground( java.awt.Color(fgCol(1), fgCol(2), fgCol(3)) )
                            jHash.put(jTicks(idx),jThisLabel);
                        end
                        
                    else
                        
                        % Manual tick labels
                        values = obj.CustomLabels.values;
                        if iscell(values)
                            values = cell2mat(values);
                        end
                        jTicks = int32(values);
                        keys = obj.CustomLabels.keys;
                        for idx=1:numel(jTicks)
                            jThisLabel = javax.swing.JLabel(keys{idx});
                            jThisLabel.setForeground( java.awt.Color(fgCol(1), fgCol(2), fgCol(3)) )
                            jHash.put(jTicks(idx),jThisLabel);
                        end
                        
                    end %if strcmpi(obj.LabelMode,'auto') || isempty(obj.CustomLabels.values)
                    
                    javaMethodEDT('setLabelTable',obj.JControl,jHash);
                    
                end %if obj.ShowTicks
                
            end %if obj.IsConstructed
        end %function
        
    end % Protected methods
    
    
    
    %% Get/Set methods
    methods
        
        function set.Min(obj,value)
            obj.Min = value;
            if obj.Max < obj.Min %#ok<MCSUP>
                obj.Max = value+1;%#ok<MCSUP>
            end
            if obj.EnforceRange && obj.Value < obj.Min %#ok<MCSUP>
                obj.Value = obj.Min;
            end
            obj.onResized();
            obj.redraw();
        end
        
        function set.Max(obj,value)
            obj.Max = value;
            if obj.Min > obj.Max %#ok<MCSUP>
                obj.Min = value-1;%#ok<MCSUP>
            end
            if obj.EnforceRange && obj.Value > obj.Max %#ok<MCSUP>
                obj.Value = obj.Max;
            end
            obj.onResized();
            obj.redraw();
        end
        
        function set.Value(this,value)
            if this.EnforceRange 
                value = max(this.Min,min(value,this.Max));
            end
            this.Value = value;
            this.onValueChanged();
        end
        
        function set.ShowTicks(obj,value)
            obj.ShowTicks = value;
            obj.redrawTicks();
        end
        
        function set.ShowLabels(obj,value)
            obj.ShowLabels = value;
            obj.redrawTicks();
        end
        
        function set.SnapToTicks(obj,value)
            obj.SnapToTicks = value;
            obj.redrawTicks();
        end
        
        function set.MinTickStep(obj,value)
            obj.MinTickStep = value;
            obj.redrawTicks();
        end
        
        function set.Orientation(obj,value)
            obj.Orientation = value;
            obj.onResized();
        end
                
        function set.Focusable(obj, value)
            obj.JControl.setFocusable(value);
            obj.Focusable = value;
        end
        
        function set.LabelMode(obj, value)
            obj.LabelMode = value;
            obj.redrawTicks();
        end
        
        function set.CustomLabels(obj, value)
            obj.CustomLabels = value;
            obj.redrawTicks();
        end
    end % Data access methods
    
end % classdef
