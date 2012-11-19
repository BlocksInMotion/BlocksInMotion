#!/bin/sh

# delete old textures
echo "deleting old high/mid/low res textures ..."
find ../data/textures \( -name "*_512.png" -o -name "*_256.png" -o -name "*_128.png" \) | xargs rm

# find all 1024px textures and start converting
textures=$(find ../data/textures -name "*_1024.png")
for i in ${textures}; do
	high_name=$(echo $i | sed s/1024/512/)
	mid_name=$(echo $i | sed s/1024/256/)
	low_name=$(echo $i | sed s/1024/128/)
	has_alpha=$(file $i | grep -c "RGBA")
	format_option="-define png:color-type="
	if [[ $has_alpha == 1 ]]; then
		format_option+="6"
	else
		format_option+="2"
	fi
	
	echo "converting "$i" => "$high_name
	convert $i -resize 50% $format_option $high_name
	echo "converting "$i" => "$mid_name
	convert $i -resize 25% $format_option $mid_name
	echo "converting "$i" => "$low_name
	convert $i -resize 12.5% $format_option $low_name
done
