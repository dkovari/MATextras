 function out=cntrd2(im,mx,sz)
% out=cntrd(im,mx,sz)
% 
% PURPOSE:  calculates the centroid of bright spots to sub-pixel accuracy.
%  Inspired by Grier & Crocker's feature for IDL, but greatly simplified and optimized
%  for matlab
% 
% INPUT:
% im: image to process, particle should be bright spots on dark background with little noise
%   ofen an bandpass filtered brightfield image or a nice fluorescent image
%
% mx: locations of local maxima to pixel-level accuracy from pkfnd.m
%
% sz: diamter of the window over which to average to calculate the centroid.  
%     should be big enough
%     to capture the whole particle but not so big that it captures others.  
%     if initial guess of center (from pkfnd) is far from the centroid, the
%     window will need to be larger than the particle size.  RECCOMMENDED
%     size is the long lengthscale used in bpass plus 2.
%     
% Requirements:
%   radialcenter() by Raghuveer Parthasarthy, Nat. Methods 2012 pp. 724-6
%   with modifications by DTK
%
% NOTE:
%  - if pkfnd, and cntrd return more then one location per particle then
%  you should try to filter your input more carefully.  If you still get
%  more than one peak for particle, use the optional sz parameter in pkfnd
%  - If you want sub-pixel accuracy, you need to have a lot of pixels in your window (sz>>1). 
%    To check for pixel bias, plot a histogram of the fractional parts of the resulting locations
%  - It is HIGHLY recommended to run in interactive mode to adjust the parameters before you
%    analyze a bunch of images.
%
% OUTPUT:  a N x 4 array containing, x, y and brightness for each feature
%           out(:,1) is the x-coordinates
%           out(:,2) is the y-coordinates
%           out(:,3) is the brightnesses
%           out(:,4) is the square of the radius of gyration
%
% CREATED: Eric R. Dufresne, Yale University, Feb 4 2005
%  5/2005 inputs diamter instead of radius
%  Modifications:
%  D.B. (6/05) Added code from imdist/dist to make this stand alone.
%  ERD (6/05) Increased frame of reject locations around edge to 1.5*sz
%  ERD 6/2005  By popular demand, 1. altered input to be formatted in x,y
%  space instead of row, column space  2. added forth column of output,
%  rg^2
%  ERD 8/05  Outputs had been shifted by [0.5,0.5] pixels.  No more!
%  ERD 8/24/05  Woops!  That last one was a red herring.  The real problem
%  is the "ringing" from the output of bpass.  I fixed bpass (see note),
%  and no longer need this kludge.  Also, made it quite nice if mx=[];
%  ERD 6/06  Added size and brightness output ot interactive mode.  Also 
%   fixed bug in calculation of rg^2
%  JWM 6/07  Small corrections to documentation
%  Jan Scrimgeour, Curtis Lab, Georgia Tech 04/13 This version of the cntrd
%  calls radialcenter.m to determine the peak position instead of using the
%  centroid method.
%  Dan Kovari, Curtis Lab, Ga Tech 02/2014
%   Cleaned up some of the syntax to be in compliance with matlabs best
%   coding practices, this should also help with speed.  Simplified some of
%   the code, and fixed the 1 pixel offset that was being introduced. Got
%   rid of interactive flag.
%  Dan Kovari 2014-04-15
%   Changed radialcenter to allow for image derivatives and filtered
%   derivatives to be pre-calculated to save on lots of calls to conv2,
%   which were very slow.  


if ~mod(sz,2)
    warning('sz must be odd, like bpass, adding one');
    sz = sz+1;
end

if isempty(mx)
    warning('there were no positions inputted into cntrd. check your pkfnd theshold')
    out=[];
    return;
end


%Only calculate points which are at locations within a distance sz from the
%edge of the image

[H,W] = size(im);
r = (sz-1)/2;
mx = mx(...
        (mx(:,2)>r & mx(:,2)<=(H-r))&...
        (mx(:,1)>r & mx(:,1)<=(W-r)),...
        :);

%Before we start the calculation, create a circular mask to apply to the windowed region  
[xx,yy] = meshgrid(-r:r,-r:r);
dst = sqrt( (xx).^2 + (yy).^2 );%keep distance matrix for calculating radius of gyration
mask = dst<= r;

%preallocate output, don't call zeros or nan, etc. because it wastes time
%filling the values
out(size(mx,1),4)=0;

%calculate image derivatives for use in radialcenter()
dIdu = im(1:end-1,2:end)-im(2:end,1:end-1);
dIdv = im(1:end-1,1:end-1)-im(2:end,2:end);
%calc smoothed derivative
h = ones(3)/9;  % simple 3x3 averaging filter
fdv = conv2(dIdv, h, 'same');
fdu = conv2(dIdu, h, 'same');

%Loop over remaining peaks and detect centroids
for p=1:size(mx,1)
    cx = (-r:r)+mx(p,1);
    cy = (-r:r)+mx(p,2);
    tmp = mask.*im(cy,cx);  %apply mask to region around peak

    %calculate radial center
    %[xavg, yavg] = radialcenter(tmp);  %old method, witout precalc. derivs
    [xavg, yavg] = radialcenter(tmp,...
        mask(1:end-1,1:end-1).*dIdu(cy(1:end-1),cx(1:end-1)),...
        mask(1:end-1,1:end-1).*dIdv(cy(1:end-1),cx(1:end-1)),...
        mask(1:end-1,1:end-1).*fdu(cy(1:end-1),cx(1:end-1)),...
        mask(1:end-1,1:end-1).*fdv(cy(1:end-1),cx(1:end-1)));
        
    %calculate integrated intensity
    norm = sum(sum(tmp));
    
    %calculate radius of gyration
    %(Rg)^2 = sum(mass_i*r_i^2) / (total mass)
    rg2 = sum(sum(tmp.*dst.^2))/norm;
    
    out(p,:) = [mx(p,1)+xavg-r-1, mx(p,2)+yavg-r-1, norm, rg2];
end

