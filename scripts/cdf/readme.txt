# Create curve in inkscape:

# Open
Copy the template.svg to a new file.
Open the new file.

# Sheet

The vertical side of the sheet is the y-axis.
The y-axis goes from 0.0 to 1.0.

The horizontal side of the sheet is the x-axis.
The x-axis range can defined later on.

# Symmetric curve

For a symmetric curve edit the current path and later on duplicate it, mirror/rotate it and move it to the respective quadrant.
Make sure the points snap in.
You can merge the two paths if you like.

# Other curves

Simply move the upper right point of the path to the upper right corner of the sheet.
Make sure to snap in with the corner.

# Edit the curve
Edit the curve as you like.
Add new points as you like.

# Linearize path
To add more line segments select a segment (select two points) and press the "+" button in the toolbar.
This will create new points.
Afterwards select all points and press the "convert to straight line" button to linearize all segments.

# Note
All path elements will be considered.
Therefore delete all unnecessary paths.

Save the file.

# Convert

Use ./svg2list.sh to get the path commands.
./svg2list.sh "foo.svg" > foo.list

Use ./list2cdf.py to convert the path to a cdf file.
./list2cdf.py foo.list 0.0 0.0 0.0 1000.0 > foo.cdf
