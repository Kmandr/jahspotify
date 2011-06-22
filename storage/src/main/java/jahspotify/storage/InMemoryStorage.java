package jahspotify.storage;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import jahspotify.media.*;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.stereotype.Service;

/**
 * @author Johan Lindquist
 */
@Service
@Qualifier(value = "in-memory")
public class InMemoryStorage implements JahStorage
{
    private Map<Link,Image> _imageStore = new ConcurrentHashMap<Link, Image>();
    private Map<Link,Track> _trackStore = new ConcurrentHashMap<Link, Track>();
    private Map<Link,Album> _albumStore = new ConcurrentHashMap<Link, Album>();
    private Map<Link,Artist> _artistStore = new ConcurrentHashMap<Link, Artist>();

    @Override
    public void store(final Track track)
    {
        _trackStore.put(track.getId(),track);
    }

    @Override
    public Track readTrack(final Link uri)
    {
        return _trackStore.get(uri);
    }

    @Override
    public void deleteTrack(final Link uri)
    {
    }

    @Override
    public void store(final Artist artist)
    {
        _artistStore.put(artist.getId(),artist);
    }

    @Override
    public Artist readArtist(final Link uri)
    {
        return _artistStore.get(uri);
    }

    @Override
    public void store(final Album album)
    {
        _albumStore.put(album.getId(),album);
    }

    @Override
    public Album readAlbum(final Link uri)
    {
        return _albumStore.get(uri);
    }

    @Override
    public void store(final Playlist playlist)
    {
    }

    @Override
    public void store(final Image image)
    {
        _imageStore.put(image.getId(),image);

    }

    @Override
    public Image readImage(final Link uri)
    {
        return _imageStore.get(uri);
    }
}