int broken(int n)
{
	if (n)
		goto done;
	return sizeof(missing_translation);
done:
	return 1;
}
